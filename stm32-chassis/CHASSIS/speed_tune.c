/**
 * @file speed_tune.c
 * @brief 电机闭环控制速度环（内环PI）参数在线调节与临时自测波形输出模块。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "speed_tune.h"
#include "chassis.h"
#include "board_config.h"

/*
 * speed_tune.c
 *
 * Tuning state manager for speed PI parameters.
 * Every parameter change is immediately pushed to the chassis layer so the
 * effect is visible on the next control loop tick without recompiling.
 */

static SpeedTuneState_t g_tune;

/* ---- Step sizes for each parameter ---- */
#define STEP_KP          10.0f
#define STEP_KI          0.1f
#define STEP_MINPWM      50
#define STEP_TGT_SPD     1
#define MAX_KP           200.0f
#define MAX_KI           10.0f

/* Clamp helpers */
static float clampf_t(float x, float lo, float hi)
{
    if (x > hi) return hi;
    if (x < lo) return lo;
    return x;
}

static int16_t clampi16_t(int16_t x, int16_t lo, int16_t hi)
{
    if (x > hi) return hi;
    if (x < lo) return lo;
    return x;
}

/* Push current PI parameters to chassis layer. */
static void apply_pid(void)
{
    Chassis_SetLeftPID(g_tune.left_kp, g_tune.left_ki);
    Chassis_SetRightPID(g_tune.right_kp, g_tune.right_ki);
}

/* Push min-PWM to chassis layer. */
static void apply_min_pwm(void)
{
    Chassis_SetMinPWM(g_tune.left_min_pwm, g_tune.right_min_pwm);
}

/* Start or stop the motor at the tuning target speed. */
static void apply_run_state(void)
{
    if (g_tune.running)
    {
        Chassis_SetSpeedCount(g_tune.target_speed, g_tune.target_speed);
    }
    else
    {
        Chassis_Stop();
    }
}

/* ---- Public API ---- */

void SpeedTune_Init(void)
{
    float kp, ki;

    /* Read current PID from chassis so the display starts with real values. */
    Chassis_GetLeftPID(&kp, &ki);
    g_tune.left_kp = kp;
    g_tune.left_ki = ki;

    Chassis_GetRightPID(&kp, &ki);
    g_tune.right_kp = kp;
    g_tune.right_ki = ki;

    Chassis_GetMinPWM(&g_tune.left_min_pwm, &g_tune.right_min_pwm);

    g_tune.target_speed = 8;     /* conservative default: ~8 counts/10ms */
    g_tune.selected = TUNE_PARAM_L_KP;
    g_tune.running = 0U;

    g_tune.left_target = 0;
    g_tune.left_actual = 0;
    g_tune.left_pwm = 0;
    g_tune.left_correction = 0;
    g_tune.left_command_pwm = 0;
    g_tune.right_target = 0;
    g_tune.right_actual = 0;
    g_tune.right_pwm = 0;
    g_tune.right_correction = 0;
    g_tune.right_command_pwm = 0;
}

void SpeedTune_NextParam(void)
{
    g_tune.selected = (TuneParamIndex_t)((g_tune.selected + 1U) % TUNE_PARAM_MAX);
}

void SpeedTune_Increase(void)
{
    switch (g_tune.selected)
    {
        case TUNE_PARAM_L_KP:
            g_tune.left_kp = clampf_t(g_tune.left_kp + STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TUNE_PARAM_L_KI:
            g_tune.left_ki = clampf_t(g_tune.left_ki + STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TUNE_PARAM_R_KP:
            g_tune.right_kp = clampf_t(g_tune.right_kp + STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TUNE_PARAM_R_KI:
            g_tune.right_ki = clampf_t(g_tune.right_ki + STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TUNE_PARAM_L_MINPWM:
            g_tune.left_min_pwm = clampi16_t(g_tune.left_min_pwm + STEP_MINPWM, 0, MOTOR_PWM_LIMIT);
            apply_min_pwm();
            break;
        case TUNE_PARAM_R_MINPWM:
            g_tune.right_min_pwm = clampi16_t(g_tune.right_min_pwm + STEP_MINPWM, 0, MOTOR_PWM_LIMIT);
            apply_min_pwm();
            break;
        case TUNE_PARAM_TGT_SPD:
            g_tune.target_speed = clampi16_t(g_tune.target_speed + STEP_TGT_SPD, 1, CHASSIS_MAX_COUNT_PER_10MS);
            if (g_tune.running) apply_run_state();
            break;
        case TUNE_PARAM_RUN_STOP:
            SpeedTune_ToggleRun();
            break;
        default:
            break;
    }
}

void SpeedTune_Decrease(void)
{
    switch (g_tune.selected)
    {
        case TUNE_PARAM_L_KP:
            g_tune.left_kp = clampf_t(g_tune.left_kp - STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TUNE_PARAM_L_KI:
            g_tune.left_ki = clampf_t(g_tune.left_ki - STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TUNE_PARAM_R_KP:
            g_tune.right_kp = clampf_t(g_tune.right_kp - STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TUNE_PARAM_R_KI:
            g_tune.right_ki = clampf_t(g_tune.right_ki - STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TUNE_PARAM_L_MINPWM:
            g_tune.left_min_pwm = clampi16_t(g_tune.left_min_pwm - STEP_MINPWM, 0, MOTOR_PWM_LIMIT);
            apply_min_pwm();
            break;
        case TUNE_PARAM_R_MINPWM:
            g_tune.right_min_pwm = clampi16_t(g_tune.right_min_pwm - STEP_MINPWM, 0, MOTOR_PWM_LIMIT);
            apply_min_pwm();
            break;
        case TUNE_PARAM_TGT_SPD:
            g_tune.target_speed = clampi16_t(g_tune.target_speed - STEP_TGT_SPD, 1, CHASSIS_MAX_COUNT_PER_10MS);
            if (g_tune.running) apply_run_state();
            break;
        case TUNE_PARAM_RUN_STOP:
            /* Run/stop is changed by short press only; a long press must
             * never start a track while the operator expects a decrement.
             */
            break;
        default:
            break;
    }
}

void SpeedTune_ToggleRun(void)
{
    if (g_tune.running)
    {
        g_tune.running = 0U;
        Chassis_Stop();
    }
    else
    {
        /* Make sure driver is powered before starting. */
        if (Chassis_IsDriverPowered())
        {
            g_tune.running = 1U;
            apply_run_state();
        }
    }
}

void SpeedTune_ForceStop(void)
{
    g_tune.running = 0U;
    Chassis_Stop();
}

void SpeedTune_RefreshMotorData(void)
{
    const ChassisState_t *s = Chassis_GetState();

    /* Read back the controller values instead of trusting only UI state. */
    Chassis_GetLeftPID(&g_tune.left_kp, &g_tune.left_ki);
    Chassis_GetRightPID(&g_tune.right_kp, &g_tune.right_ki);
    Chassis_GetMinPWM(&g_tune.left_min_pwm, &g_tune.right_min_pwm);

    g_tune.left_target  = s->target_left_count;
    g_tune.left_actual  = s->enc_left;
    g_tune.left_pwm     = s->pwm_left;
    g_tune.left_correction = s->pi_correction_left;
    g_tune.left_command_pwm = s->speed_cmd_pwm_left;
    g_tune.right_target = s->target_right_count;
    g_tune.right_actual = s->enc_right;
    g_tune.right_pwm    = s->pwm_right;
    g_tune.right_correction = s->pi_correction_right;
    g_tune.right_command_pwm = s->speed_cmd_pwm_right;

    /* If chassis was externally stopped (e.g. estop), sync our state. */
    if (s->mode == CHASSIS_MODE_STOP && g_tune.running)
    {
        g_tune.running = 0U;
    }
}

const SpeedTuneState_t *SpeedTune_GetState(void)
{
    return &g_tune;
}
