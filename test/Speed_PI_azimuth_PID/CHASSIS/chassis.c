#include "chassis.h"
#include "board_config.h"
#include "motor_driver.h"
#include "encoder_driver.h"
#include "pid.h"
#include "battery.h"
#include "mpu6050.h"
#include "stm32f1xx_hal.h"

/*
 * chassis.c - safe test version
 *
 * The main protection idea is conservative software limiting + soft ramping:
 * 1. User commands are clamped before they enter the controller.
 * 2. Non-zero open-loop PWM below MOTOR_PWM_MIN_EFFECTIVE is lifted to a value
 *    that is more likely to make the motor move instead of buzzing/stalling.
 *    The same per-side value is used as speed-loop start feed-forward, so PI
 *    tuning does not depend on a long integral wind-up before tracks move.
 * 3. PWM and speed-count targets ramp gradually every 10 ms.  This prevents a
 *    sudden current spike when testing tracks for the first time.
 * 4. D153B power is checked from PA5/ADC before any motor output is allowed.
 *    If driver power is off, PWM and direction pins are switched to analog
 *    high-impedance to stop GPIO back-feeding into the unpowered driver.
 *
 * This does NOT replace hardware protection.  During the first test:
 * - lift the chassis so tracks are off the table,
 * - start with `test low`,
 * - touch the TB6612/D153B and motor carefully to check temperature,
 * - stop immediately if the motor stalls, buzzes, or the driver heats quickly.
 */

static ChassisState_t g_chassis;
static PID_Inc_t g_pid_left;
static PID_Inc_t g_pid_right;
PID_Inc_t g_pid_turn;
float g_target_yaw = 0.0f;
static float g_yaw_offset = 0.0f;
int16_t g_auto_target_speed = 10;



/* Runtime-adjustable minimum effective PWM per side.
 * Initialized from the compile-time MOTOR_PWM_MIN_EFFECTIVE in board_config.h.
 * Can be changed at run time via Chassis_SetMinPWM() for tuning.
 */
static int16_t g_min_pwm_left;
static int16_t g_min_pwm_right;
static uint16_t g_start_boost_left_ticks;
static uint16_t g_start_boost_right_ticks;

static int16_t clampi16_local(int16_t x, int16_t min_v, int16_t max_v);

volatile ChassisStlinkTuneBlock_t g_stlink_tune =
{
    STLINK_TUNE_MAGIC,
    STLINK_TUNE_VERSION,
    sizeof(ChassisStlinkTuneBlock_t),
    0U,
    0U,
    STLINK_TUNE_CMD_NONE,
    0U,
    (int32_t)(SPEED_PID_DEFAULT_KP * 100.0f),
    (int32_t)(SPEED_PID_DEFAULT_KP * 100.0f),
    (int32_t)(SPEED_PID_DEFAULT_KI * 1000.0f),
    (int32_t)(SPEED_PID_DEFAULT_KI * 1000.0f),
    MOTOR_LEFT_FF_DEFAULT,
    MOTOR_RIGHT_FF_DEFAULT,
    5,
    0U,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0U, 0U
};

static int16_t float_to_i16(float x)
{
    return (x >= 0.0f) ? (int16_t)(x + 0.5f) : (int16_t)(x - 0.5f);
}

static float clampf_local(float x, float min_v, float max_v)
{
    if (x > max_v) return max_v;
    if (x < min_v) return min_v;
    return x;
}

static int16_t clampi16_local(int16_t x, int16_t min_v, int16_t max_v)
{
    if (x > max_v) return max_v;
    if (x < min_v) return min_v;
    return x;
}

static int16_t abs_i16(int16_t x)
{
    return (x < 0) ? (int16_t)(-x) : x;
}

static int16_t sign_i16(int16_t x)
{
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

static int16_t ramp_i16(int16_t current, int16_t target, int16_t step)
{
    int16_t delta = target - current;

    if (step <= 0) return target;

    if (delta > step) return (int16_t)(current + step);
    if (delta < -step) return (int16_t)(current - step);
    return target;
}

/* Clamp speed-count target.
 *
 * Very tiny non-zero targets often make a geared DC motor hunt around zero or
 * fail to start.  Therefore a non-zero target is lifted to
 * CHASSIS_MIN_COUNT_PER_10MS.  If you need ultra-low-speed crawling later, lower
 * the minimum after the whole drive system is verified.
 */
static int16_t limit_count_command(int16_t value)
{
    int16_t mag = abs_i16(value);

    if (mag > CHASSIS_MAX_COUNT_PER_10MS)
    {
        g_chassis.command_limited = 1U;
        return (int16_t)(sign_i16(value) * CHASSIS_MAX_COUNT_PER_10MS);
    }

    if (mag > 0 && mag < CHASSIS_MIN_COUNT_PER_10MS)
    {
        g_chassis.command_limited = 1U;
        return (int16_t)(sign_i16(value) * CHASSIS_MIN_COUNT_PER_10MS);
    }

    return value;
}

/* Clamp open-loop PWM.
 *
 * Open loop is only for wiring tests.  The limit is deliberately much smaller
 * than the timer period.  Use closed loop (`spd` or `vel`) for normal testing.
 */
static int16_t limit_pwm_command(int16_t value)
{
    int16_t mag = abs_i16(value);
    /* Use the larger of the two per-side min values as a common floor.
     * Open-loop commands apply to both sides, so use the max.
     */
    int16_t min_eff = (g_min_pwm_left > g_min_pwm_right) ? g_min_pwm_left : g_min_pwm_right;

    if (mag > MOTOR_PWM_LIMIT)
    {
        g_chassis.command_limited = 1U;
        return (int16_t)(sign_i16(value) * MOTOR_PWM_LIMIT);
    }

    if (mag > 0 && mag < min_eff)
    {
        g_chassis.command_limited = 1U;
        return (int16_t)(sign_i16(value) * min_eff);
    }

    return value;
}

/* Add the minimum useful start drive as closed-loop feed-forward.
 *
 * PID output remains a correction around this per-side drive value, allowing
 * it to reduce PWM below the start level after the track is moving.  During
 * raised-chassis tuning, do not command an active reversal merely because one
 * side briefly overspeeds; coasting to zero output is the safer response.
 */
static int16_t add_speed_feedforward(int16_t pid_pwm, int16_t target, int16_t start_pwm)
{
    int32_t output = (int32_t)pid_pwm;

    if (target > 0)
    {
        output += start_pwm;
        if (output < 0) output = 0;
    }
    else if (target < 0)
    {
        output -= start_pwm;
        if (output > 0) output = 0;
    }

    if (output > MOTOR_PWM_LIMIT) output = MOTOR_PWM_LIMIT;
    if (output < -MOTOR_PWM_LIMIT) output = -MOTOR_PWM_LIMIT;
    return (int16_t)output;
}

static int16_t apply_start_boost(int16_t cmd_pwm,
                                 int16_t target,
                                 int16_t boost_pwm,
                                 uint16_t *boost_ticks)
{
    int32_t output = cmd_pwm;

    if (target == 0)
    {
        *boost_ticks = 0U;
        return cmd_pwm;
    }

    if (*boost_ticks == 0U)
    {
        return cmd_pwm;
    }

    (*boost_ticks)--;

    if (target > 0)
    {
        if (output < boost_pwm) output = boost_pwm;
    }
    else
    {
        if (output > -boost_pwm) output = -boost_pwm;
    }

    if (output > MOTOR_PWM_LIMIT) output = MOTOR_PWM_LIMIT;
    if (output < -MOTOR_PWM_LIMIT) output = -MOTOR_PWM_LIMIT;
    return (int16_t)output;
}

static void stlink_tune_update_status(void)
{
    g_stlink_tune.magic = STLINK_TUNE_MAGIC;
    g_stlink_tune.version = STLINK_TUNE_VERSION;
    g_stlink_tune.size = sizeof(ChassisStlinkTuneBlock_t);
    g_stlink_tune.tick = g_chassis.control_tick;
    g_stlink_tune.mode = (int32_t)g_chassis.mode;
    g_stlink_tune.left_target = g_chassis.target_left_count;
    g_stlink_tune.left_actual = g_chassis.enc_left;
    g_stlink_tune.left_pwm = g_chassis.pwm_left;
    g_stlink_tune.left_correction = g_chassis.pi_correction_left;
    g_stlink_tune.left_cmd_pwm = g_chassis.speed_cmd_pwm_left;
    g_stlink_tune.right_target = g_chassis.target_right_count;
    g_stlink_tune.right_actual = g_chassis.enc_right;
    g_stlink_tune.right_pwm = g_chassis.pwm_right;
    g_stlink_tune.right_correction = g_chassis.pi_correction_right;
    g_stlink_tune.right_cmd_pwm = g_chassis.speed_cmd_pwm_right;
    g_stlink_tune.left_total = g_chassis.enc_left_total;
    g_stlink_tune.right_total = g_chassis.enc_right_total;
    g_stlink_tune.battery_mv = (int32_t)(g_chassis.battery_v * 1000.0f);
    g_stlink_tune.fault_code = g_chassis.fault_code;
    g_stlink_tune.driver_powered = g_chassis.driver_powered;
}

static void stlink_tune_apply_params(void)
{
    float left_kp;
    float right_kp;
    float left_ki;
    float right_ki;
    int16_t left_ff;
    int16_t right_ff;

    left_kp = ((float)g_stlink_tune.left_kp_x100) / 100.0f;
    right_kp = ((float)g_stlink_tune.right_kp_x100) / 100.0f;
    left_ki = ((float)g_stlink_tune.left_ki_x1000) / 1000.0f;
    right_ki = ((float)g_stlink_tune.right_ki_x1000) / 1000.0f;
    left_ff = clampi16_local((int16_t)g_stlink_tune.left_ff, 0, MOTOR_PWM_LIMIT);
    right_ff = clampi16_local((int16_t)g_stlink_tune.right_ff, 0, MOTOR_PWM_LIMIT);

    Chassis_SetLeftPID(left_kp, left_ki);
    Chassis_SetRightPID(right_kp, right_ki);
    Chassis_SetMinPWM(left_ff, right_ff);
}

static void stlink_tune_process_command(void)
{
    uint32_t command;
    int16_t target;

    if (g_stlink_tune.magic != STLINK_TUNE_MAGIC)
    {
        return;
    }

    if (g_stlink_tune.seq == g_stlink_tune.ack_seq)
    {
        return;
    }

    command = g_stlink_tune.command;
    target = clampi16_local((int16_t)g_stlink_tune.target_count,
                            -CHASSIS_MAX_COUNT_PER_10MS,
                            CHASSIS_MAX_COUNT_PER_10MS);

    switch (command)
    {
        case STLINK_TUNE_CMD_APPLY:
            stlink_tune_apply_params();
            break;

        case STLINK_TUNE_CMD_RUN:
            stlink_tune_apply_params();
            g_stlink_tune.run = 1U;
            Chassis_SetSpeedCount(target, target);
            break;

        case STLINK_TUNE_CMD_STOP:
            g_stlink_tune.run = 0U;
            Chassis_Stop();
            break;

        case STLINK_TUNE_CMD_CLEARFAULT:
            Chassis_ClearFault();
            break;

        default:
            break;
    }

    g_stlink_tune.ack_seq = g_stlink_tune.seq;
    g_stlink_tune.command = STLINK_TUNE_CMD_NONE;
}

/* A displayed zero gain must mean an electrically pure feed-forward test.
 * Apart from making FF calibration unambiguous, this clears any old integral
 * correction if gains are changed to zero while a tuning run is active.
 */
static int16_t calc_speed_pi_correction(PID_Inc_t *pid, int16_t target, int16_t feedback)
{
    if (pid->kp < 0.0005f && pid->ki < 0.0005f && pid->kd < 0.0005f)
    {
        PID_IncReset(pid);
        return 0;
    }

    return float_to_i16(PID_IncCalc(pid, (float)target, (float)feedback));
}

/* Convert body velocity command to left/right track target counts per control
 * period.  v_mm_s is forward linear speed.  w_mrad_s is yaw rate in mrad/s.
 * Positive w makes the right track faster than the left track.
 */
static void calc_target_from_velocity(float v_mm_s,
                                      float w_mrad_s,
                                      int16_t *left_target,
                                      int16_t *right_target)
{
    float omega_rad_s = w_mrad_s / 1000.0f;
    float left_mm_s;
    float right_mm_s;

    left_mm_s  = v_mm_s - omega_rad_s * TRACK_WIDTH_MM * 0.5f;
    right_mm_s = v_mm_s + omega_rad_s * TRACK_WIDTH_MM * 0.5f;

    *left_target  = limit_count_command(float_to_i16(left_mm_s  * CONTROL_PERIOD_S / MM_PER_ENCODER_COUNT));
    *right_target = limit_count_command(float_to_i16(right_mm_s * CONTROL_PERIOD_S / MM_PER_ENCODER_COUNT));
}

static void set_cmd_alive(void)
{
    g_chassis.last_cmd_tick_ms = HAL_GetTick();
}

/*
 * Update D153B power state from the ADC voltage sample.
 *
 * This function is the software key to solving the dim LED/back-feed problem:
 * when D153B VM is absent, we refuse motor commands and call
 * Motor_DisableOutputsHiZ(), so PA6/PA7/PE0/PE1/PE2/PE6 no longer drive any
 * high level into the unpowered D153B input pins.
 *
 * Hysteresis prevents ON/OFF flicker near the threshold.
 */
void Chassis_RefreshDriverPower(void)
{
#if DRIVER_POWER_CHECK_ENABLE
    float v = Battery_ReadVoltage();
    g_chassis.battery_v = v;

    if (v >= DRIVER_POWER_ON_VOLTAGE)
    {
        g_chassis.driver_powered = 1U;
    }
    else if (v <= DRIVER_POWER_OFF_VOLTAGE)
    {
        g_chassis.driver_powered = 0U;
    }
#else
    g_chassis.driver_powered = 1U;
#endif
}

uint8_t Chassis_IsDriverPowered(void)
{
    Chassis_RefreshDriverPower();
    return g_chassis.driver_powered;
}

static uint8_t safety_check(void)
{
#if CHASSIS_CMD_TIMEOUT_MS > 0
    if (g_chassis.mode != CHASSIS_MODE_STOP &&
        (HAL_GetTick() - g_chassis.last_cmd_tick_ms) > CHASSIS_CMD_TIMEOUT_MS)
    {
        g_chassis.fault_code |= CHASSIS_FAULT_CMD_TIMEOUT;
        return 0U;
    }
#endif

#if BATTERY_ENABLE_PROTECTION
    if ((g_chassis.battery_v > 1.0f) && (g_chassis.battery_v < BATTERY_LOW_VOLTAGE))
    {
        g_chassis.fault_code |= CHASSIS_FAULT_LOW_BATTERY;
        return 0U;
    }
#endif

    if (g_chassis.fault_code != CHASSIS_FAULT_NONE) return 0U;
    return 1U;
}

static void reset_runtime_targets(void)
{
    g_chassis.desired_left_count = 0;
    g_chassis.desired_right_count = 0;
    g_chassis.target_left_count = 0;
    g_chassis.target_right_count = 0;
    g_chassis.desired_pwm_left = 0;
    g_chassis.desired_pwm_right = 0;
    g_chassis.pwm_left = 0;
    g_chassis.pwm_right = 0;
    g_chassis.pi_correction_left = 0;
    g_chassis.pi_correction_right = 0;
    g_chassis.speed_cmd_pwm_left = 0;
    g_chassis.speed_cmd_pwm_right = 0;
    g_start_boost_left_ticks = 0U;
    g_start_boost_right_ticks = 0U;
}

void Chassis_Init(void)
{
    Motor_Init();
    Encoder_Init();

    g_min_pwm_left  = MOTOR_LEFT_FF_DEFAULT;
    g_min_pwm_right = MOTOR_RIGHT_FF_DEFAULT;

    /* Ground-test baseline.  Start boost handles static friction; the steady
     * speed loop uses a lower FF plus this conservative proportional gain.
     */
    PID_IncInit(&g_pid_left,  SPEED_PID_DEFAULT_KP, SPEED_PID_DEFAULT_KI, 0.0f, -MOTOR_PWM_LIMIT, MOTOR_PWM_LIMIT);
    PID_IncInit(&g_pid_right, SPEED_PID_DEFAULT_KP, SPEED_PID_DEFAULT_KI, 0.0f, -MOTOR_PWM_LIMIT, MOTOR_PWM_LIMIT);
    PID_IncInit(&g_pid_turn,  TURN_PID_DEFAULT_KP, TURN_PID_DEFAULT_KI, TURN_PID_DEFAULT_KD, -50.0f, 50.0f);

    g_chassis.mode = CHASSIS_MODE_STOP;
    g_chassis.enc_left = 0;
    g_chassis.enc_right = 0;
    g_chassis.enc_left_total = 0;
    g_chassis.enc_right_total = 0;
    reset_runtime_targets();
    g_chassis.cmd_v_mm_s = 0.0f;
    g_chassis.cmd_w_mrad_s = 0.0f;
    g_chassis.battery_v = 0.0f;
    g_chassis.control_tick = 0U;
    g_chassis.last_cmd_tick_ms = HAL_GetTick();
    g_chassis.fault_code = CHASSIS_FAULT_NONE;
    g_chassis.command_limited = 0U;
    g_chassis.driver_powered = 0U;
    g_chassis.driver_blocked = 0U;

    Chassis_RefreshDriverPower();
    if (g_chassis.driver_powered)
    {
        Motor_Stop();
    }
    else
    {
        Motor_DisableOutputsHiZ();
    }
    Chassis_LoadParamsFromFlash();
}


void Chassis_ControlLoop10ms(void)
{
    int16_t pid_cmd_left;
    int16_t pid_cmd_right;
    int16_t ramp_step_left;
    int16_t ramp_step_right;
    uint8_t boost_left_active;
    uint8_t boost_right_active;
    int16_t turn_corr = 0;

    g_chassis.control_tick++;

    /* Read latest MPU6050 posture every tick to update Yaw continuously and prevent FIFO overflow */
    Read_DMP();

    /* Read encoder delta first.  The value is the count within this 10 ms. */
    g_chassis.enc_left = Encoder_ReadLeftDelta();
    g_chassis.enc_right = Encoder_ReadRightDelta();
    /* Totals are for bring-up diagnostics only; PI still uses the 10 ms delta. */
    g_chassis.enc_left_total += g_chassis.enc_left;
    g_chassis.enc_right_total += g_chassis.enc_right;

    stlink_tune_process_command();

    /* Check D153B power every control tick.  ADC conversion is short enough for
     * a 10 ms loop, and fast detection keeps the output pins in Hi-Z as soon as
     * the driver power is removed.
     */
    Chassis_RefreshDriverPower();

    if (!g_chassis.driver_powered)
    {
        g_chassis.driver_blocked = 1U;
        g_chassis.mode = CHASSIS_MODE_STOP;
        reset_runtime_targets();
        Motor_DisableOutputsHiZ();
        PID_IncReset(&g_pid_left);
        PID_IncReset(&g_pid_right);
        stlink_tune_update_status();
        return;
    }
    g_chassis.driver_blocked = 0U;

    if (!safety_check())
    {
        reset_runtime_targets();
        Motor_Stop();
        PID_IncReset(&g_pid_left);
        PID_IncReset(&g_pid_right);
        stlink_tune_update_status();
        return;
    }

    switch (g_chassis.mode)
    {
        case CHASSIS_MODE_OPEN_LOOP:
            g_chassis.pi_correction_left = 0;
            g_chassis.pi_correction_right = 0;
            g_chassis.speed_cmd_pwm_left = g_chassis.desired_pwm_left;
            g_chassis.speed_cmd_pwm_right = g_chassis.desired_pwm_right;
            /* Open-loop PWM is ramped directly. */
            g_chassis.pwm_left = ramp_i16(g_chassis.pwm_left,
                                          g_chassis.desired_pwm_left,
                                          MOTOR_PWM_RAMP_STEP);
            g_chassis.pwm_right = ramp_i16(g_chassis.pwm_right,
                                           g_chassis.desired_pwm_right,
                                           MOTOR_PWM_RAMP_STEP);
            Motor_SetPWM(g_chassis.pwm_left, g_chassis.pwm_right);
            break;

        case CHASSIS_MODE_SPEED_COUNT:
        case CHASSIS_MODE_VEL_MM:
        case CHASSIS_MODE_HEADING:
            /* Closed-loop target speed is ramped before entering PI. */
            g_chassis.target_left_count = ramp_i16(g_chassis.target_left_count,
                                                   g_chassis.desired_left_count,
                                                   CHASSIS_COUNT_RAMP_STEP);
            g_chassis.target_right_count = ramp_i16(g_chassis.target_right_count,
                                                    g_chassis.desired_right_count,
                                                    CHASSIS_COUNT_RAMP_STEP);

            if (g_chassis.target_left_count != 0 &&
                g_chassis.pwm_left == 0 &&
                g_start_boost_left_ticks == 0U)
            {
                g_start_boost_left_ticks = MOTOR_START_BOOST_TICKS;
            }

            if (g_chassis.target_right_count != 0 &&
                g_chassis.pwm_right == 0 &&
                g_start_boost_right_ticks == 0U)
            {
                g_start_boost_right_ticks = MOTOR_START_BOOST_TICKS;
            }

            turn_corr = 0;
            if (g_chassis.mode == CHASSIS_MODE_HEADING)
            {
                /* Absolute heading tracking */
                float yaw_err = g_target_yaw - Chassis_GetYaw();
                /* Normalize error to [-180, 180] */
                while (yaw_err > 180.0f) yaw_err -= 360.0f;
                while (yaw_err < -180.0f) yaw_err += 360.0f;
                
                turn_corr = -float_to_i16(PID_IncCalc(&g_pid_turn, yaw_err, 0.0f)); 
                
                /* Apply Small-Error Boost to overcome track static friction */
                if (yaw_err > 1.5f)
                {
                    if (turn_corr > -2) turn_corr = -2;
                }
                else if (yaw_err < -1.5f)
                {
                    if (turn_corr < 2) turn_corr = 2;
                }
                else
                {
                    turn_corr = 0;
                    PID_IncReset(&g_pid_turn);
                }
            }
            else if (g_chassis.desired_left_count == g_chassis.desired_right_count && g_chassis.desired_left_count != 0)
            {
                /* Basic straight line heading hold */
                float yaw_err = g_target_yaw - Chassis_GetYaw();
                while (yaw_err > 180.0f) yaw_err -= 360.0f;
                while (yaw_err < -180.0f) yaw_err += 360.0f;
                turn_corr = -float_to_i16(PID_IncCalc(&g_pid_turn, yaw_err, 0.0f));
                
                /* Apply Small-Error Boost to overcome track static friction */
                if (yaw_err > 1.5f)
                {
                    if (turn_corr > -2) turn_corr = -2;
                }
                else if (yaw_err < -1.5f)
                {
                    if (turn_corr < 2) turn_corr = 2;
                }
                else
                {
                    turn_corr = 0;
                    PID_IncReset(&g_pid_turn);
                }
            }
            else
            {
                /* Turning manually or stopped: track current yaw, reset PID */
                g_target_yaw = Chassis_GetYaw();
                PID_IncReset(&g_pid_turn);
            }


            g_chassis.pi_correction_left = calc_speed_pi_correction(&g_pid_left,
                                                                    g_chassis.target_left_count + turn_corr,
                                                                    g_chassis.enc_left);
            g_chassis.pi_correction_right = calc_speed_pi_correction(&g_pid_right,
                                                                     g_chassis.target_right_count - turn_corr,
                                                                     g_chassis.enc_right);

            pid_cmd_left = add_speed_feedforward(g_chassis.pi_correction_left,
                                                 g_chassis.target_left_count,
                                                 g_min_pwm_left);
            pid_cmd_right = add_speed_feedforward(g_chassis.pi_correction_right,
                                                  g_chassis.target_right_count,
                                                  g_min_pwm_right);

            boost_left_active = (g_start_boost_left_ticks > 0U) ? 1U : 0U;
            boost_right_active = (g_start_boost_right_ticks > 0U) ? 1U : 0U;

            pid_cmd_left = apply_start_boost(pid_cmd_left,
                                             g_chassis.target_left_count,
                                             MOTOR_LEFT_START_BOOST_PWM,
                                             &g_start_boost_left_ticks);
            pid_cmd_right = apply_start_boost(pid_cmd_right,
                                              g_chassis.target_right_count,
                                              MOTOR_RIGHT_START_BOOST_PWM,
                                              &g_start_boost_right_ticks);

            g_chassis.speed_cmd_pwm_left = pid_cmd_left;
            g_chassis.speed_cmd_pwm_right = pid_cmd_right;

            /* Final PWM is also ramped.  This is intentionally conservative for
             * first tests and reduces current shock when a track touches ground.
             */
            ramp_step_left = (boost_left_active != 0U) ? MOTOR_START_BOOST_RAMP_STEP : MOTOR_PWM_RAMP_STEP;
            ramp_step_right = (boost_right_active != 0U) ? MOTOR_START_BOOST_RAMP_STEP : MOTOR_PWM_RAMP_STEP;
            g_chassis.pwm_left = ramp_i16(g_chassis.pwm_left, pid_cmd_left, ramp_step_left);
            g_chassis.pwm_right = ramp_i16(g_chassis.pwm_right, pid_cmd_right, ramp_step_right);

            Motor_SetPWM(g_chassis.pwm_left, g_chassis.pwm_right);
            break;

        case CHASSIS_MODE_STOP:
        default:
            reset_runtime_targets();
            Motor_Stop();
            PID_IncReset(&g_pid_left);
            PID_IncReset(&g_pid_right);
            break;
    }

    stlink_tune_update_status();
}

void Chassis_SetOpenLoopPWM(int16_t left_pwm, int16_t right_pwm)
{
    g_chassis.command_limited = 0U;

    if (!Chassis_IsDriverPowered())
    {
        g_chassis.driver_blocked = 1U;
        g_chassis.mode = CHASSIS_MODE_STOP;
        reset_runtime_targets();
        Motor_DisableOutputsHiZ();
        set_cmd_alive();
        return;
    }

    g_chassis.driver_blocked = 0U;
    g_chassis.mode = CHASSIS_MODE_OPEN_LOOP;
    g_chassis.desired_pwm_left = limit_pwm_command(left_pwm);
    g_chassis.desired_pwm_right = limit_pwm_command(right_pwm);
    g_chassis.cmd_v_mm_s = 0.0f;
    g_chassis.cmd_w_mrad_s = 0.0f;
    set_cmd_alive();
}

void Chassis_SetSpeedCount(int16_t left_count_per_period, int16_t right_count_per_period)
{
    g_chassis.command_limited = 0U;

    if (!Chassis_IsDriverPowered())
    {
        g_chassis.driver_blocked = 1U;
        g_chassis.mode = CHASSIS_MODE_STOP;
        reset_runtime_targets();
        Motor_DisableOutputsHiZ();
        set_cmd_alive();
        return;
    }

    g_chassis.driver_blocked = 0U;
    g_chassis.mode = CHASSIS_MODE_SPEED_COUNT;
    g_chassis.desired_left_count = limit_count_command(left_count_per_period);
    g_chassis.desired_right_count = limit_count_command(right_count_per_period);
    g_chassis.cmd_v_mm_s = 0.0f;
    g_chassis.cmd_w_mrad_s = 0.0f;
    set_cmd_alive();
}

void Chassis_SetVelocity(float v_mm_s, float w_mrad_s)
{
    float limited_v;
    float limited_w;

    g_chassis.command_limited = 0U;

    if (!Chassis_IsDriverPowered())
    {
        g_chassis.driver_blocked = 1U;
        g_chassis.mode = CHASSIS_MODE_STOP;
        reset_runtime_targets();
        Motor_DisableOutputsHiZ();
        set_cmd_alive();
        return;
    }

    g_chassis.driver_blocked = 0U;

    limited_v = clampf_local(v_mm_s, -CHASSIS_MAX_V_MM_S, CHASSIS_MAX_V_MM_S);
    limited_w = clampf_local(w_mrad_s, -CHASSIS_MAX_W_MRAD_S, CHASSIS_MAX_W_MRAD_S);

    if (limited_v != v_mm_s || limited_w != w_mrad_s)
    {
        g_chassis.command_limited = 1U;
    }

    g_chassis.mode = CHASSIS_MODE_VEL_MM;
    g_chassis.cmd_v_mm_s = limited_v;
    g_chassis.cmd_w_mrad_s = limited_w;
    calc_target_from_velocity(limited_v, limited_w,
                              &g_chassis.desired_left_count,
                              &g_chassis.desired_right_count);
    set_cmd_alive();
}

void Chassis_SetAbsoluteHeading(float target_yaw_degrees, int16_t forward_speed_count)
{
    g_chassis.command_limited = 0U;

    if (!Chassis_IsDriverPowered())
    {
        g_chassis.driver_blocked = 1U;
        g_chassis.mode = CHASSIS_MODE_STOP;
        reset_runtime_targets();
        Motor_DisableOutputsHiZ();
        set_cmd_alive();
        return;
    }

    g_chassis.driver_blocked = 0U;
    g_chassis.mode = CHASSIS_MODE_HEADING;
    g_target_yaw = target_yaw_degrees;
    
    /* Set the base forward speed, turn_corr will handle the steering */
    g_chassis.desired_left_count = limit_count_command(forward_speed_count);
    g_chassis.desired_right_count = limit_count_command(forward_speed_count);
    g_chassis.cmd_v_mm_s = 0.0f;
    g_chassis.cmd_w_mrad_s = 0.0f;
    set_cmd_alive();
}

void Chassis_Stop(void)
{
    g_chassis.mode = CHASSIS_MODE_STOP;
    g_chassis.cmd_v_mm_s = 0.0f;
    g_chassis.cmd_w_mrad_s = 0.0f;
    reset_runtime_targets();

    Chassis_RefreshDriverPower();
    if (g_chassis.driver_powered)
    {
        Motor_Stop();
    }
    else
    {
        Motor_DisableOutputsHiZ();
    }

    PID_IncReset(&g_pid_left);
    PID_IncReset(&g_pid_right);
    set_cmd_alive();
}

void Chassis_EStop(void)
{
    g_chassis.fault_code |= CHASSIS_FAULT_ESTOP;
    g_chassis.mode = CHASSIS_MODE_STOP;
    reset_runtime_targets();

    Chassis_RefreshDriverPower();
    if (g_chassis.driver_powered)
    {
        Motor_Stop();
    }
    else
    {
        Motor_DisableOutputsHiZ();
    }
}

void Chassis_ClearFault(void)
{
    g_chassis.fault_code = CHASSIS_FAULT_NONE;
    Chassis_Stop();
}

void Chassis_SetPID(float kp, float ki, float kd)
{
    PID_IncSetParam(&g_pid_left, kp, ki, kd);
    PID_IncSetParam(&g_pid_right, kp, ki, kd);
    PID_IncReset(&g_pid_left);
    PID_IncReset(&g_pid_right);
}

const ChassisState_t *Chassis_GetState(void)
{
    return &g_chassis;
}

/* ---- Per-side PID and MinPWM access for speed tuning ---- */

void Chassis_SetLeftPID(float kp, float ki)
{
    PID_IncSetParam(&g_pid_left, kp, ki, 0.0f);
    PID_IncReset(&g_pid_left);
}

void Chassis_SetRightPID(float kp, float ki)
{
    PID_IncSetParam(&g_pid_right, kp, ki, 0.0f);
    PID_IncReset(&g_pid_right);
}

void Chassis_GetLeftPID(float *kp, float *ki)
{
    if (kp) *kp = g_pid_left.kp;
    if (ki) *ki = g_pid_left.ki;
}

void Chassis_GetRightPID(float *kp, float *ki)
{
    if (kp) *kp = g_pid_right.kp;
    if (ki) *ki = g_pid_right.ki;
}

void Chassis_SetMinPWM(int16_t left_min, int16_t right_min)
{
    g_min_pwm_left  = left_min;
    g_min_pwm_right = right_min;
}

void Chassis_GetMinPWM(int16_t *left_min, int16_t *right_min)
{
    if (left_min)  *left_min  = g_min_pwm_left;
    if (right_min) *right_min = g_min_pwm_right;
}

void Chassis_SetTurnPID(float kp, float ki, float kd)
{
    PID_IncSetParam(&g_pid_turn, kp, ki, kd);
    PID_IncReset(&g_pid_turn);
}

void Chassis_GetTurnPID(float *kp, float *ki, float *kd)
{
    if (kp) *kp = g_pid_turn.kp;
    if (ki) *ki = g_pid_turn.ki;
    if (kd) *kd = g_pid_turn.kd;
}

/* Flash persistence structure. Fits perfectly into 11 32-bit words (44 bytes). */
typedef struct
{
    uint32_t magic;              /* 0x5A5A5A5A */
    float speed_kp_l;
    float speed_ki_l;
    float speed_kp_r;
    float speed_ki_r;
    int16_t min_pwm_l;
    int16_t min_pwm_r;
    float turn_kp;
    float turn_ki;
    float turn_kd;
    int16_t target_speed;
    int16_t reserved;            /* padding for 32-bit alignment */
    uint32_t checksum;           /* simple sum of all previous words */
} SavedParams_t;

#define FLASH_SAVE_ADDR   0x0807F800U

float Chassis_GetYaw(void)
{
    float y = Yaw - g_yaw_offset;
    while (y > 180.0f) y -= 360.0f;
    while (y < -180.0f) y += 360.0f;
    return y;
}

void Chassis_ZeroYaw(void)
{
    g_yaw_offset = Yaw;
}

void Chassis_SaveParamsToFlash(void)
{
    SavedParams_t params;
    uint32_t *p_data;
    uint32_t addr;
    uint32_t i;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    uint32_t checksum = 0;

    params.magic = 0x5A5A5A5A;
    params.speed_kp_l = g_pid_left.kp;
    params.speed_ki_l = g_pid_left.ki;
    params.speed_kp_r = g_pid_right.kp;
    params.speed_ki_r = g_pid_right.ki;
    params.min_pwm_l = g_min_pwm_left;
    params.min_pwm_r = g_min_pwm_right;
    params.turn_kp = g_pid_turn.kp;
    params.turn_ki = g_pid_turn.ki;
    params.turn_kd = g_pid_turn.kd;
    params.target_speed = g_auto_target_speed;
    params.reserved = 0;

    p_data = (uint32_t *)&params;
    for (i = 0U; i < (sizeof(SavedParams_t) / 4U) - 1U; i++)
    {
        checksum += p_data[i];
    }
    params.checksum = checksum;

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_SAVE_ADDR;
    erase_init.NbPages = 1;
    HAL_FLASHEx_Erase(&erase_init, &page_error);

    addr = FLASH_SAVE_ADDR;
    p_data = (uint32_t *)&params;
    for (i = 0U; i < (sizeof(SavedParams_t) / 4U); i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, p_data[i]);
        addr += 4U;
    }

    HAL_FLASH_Lock();
}

void Chassis_LoadParamsFromFlash(void)
{
    SavedParams_t params;
    uint32_t *p_data;
    uint32_t addr;
    uint32_t i;
    uint32_t checksum = 0;

    addr = FLASH_SAVE_ADDR;
    p_data = (uint32_t *)&params;
    for (i = 0U; i < (sizeof(SavedParams_t) / 4U); i++)
    {
        p_data[i] = *(__IO uint32_t *)addr;
        addr += 4U;
    }

    if (params.magic != 0x5A5A5A5A)
    {
        return;
    }

    for (i = 0U; i < (sizeof(SavedParams_t) / 4U) - 1U; i++)
    {
        checksum += p_data[i];
    }
    if (checksum != params.checksum)
    {
        return;
    }

    g_min_pwm_left  = params.min_pwm_l;
    g_min_pwm_right = params.min_pwm_r;
    g_auto_target_speed = params.target_speed;

    PID_IncInit(&g_pid_left,  params.speed_kp_l, params.speed_ki_l, 0.0f, -MOTOR_PWM_LIMIT, MOTOR_PWM_LIMIT);
    PID_IncInit(&g_pid_right, params.speed_kp_r, params.speed_ki_r, 0.0f, -MOTOR_PWM_LIMIT, MOTOR_PWM_LIMIT);
    PID_IncInit(&g_pid_turn,  params.turn_kp,    params.turn_ki,    params.turn_kd, -50.0f, 50.0f);
}

