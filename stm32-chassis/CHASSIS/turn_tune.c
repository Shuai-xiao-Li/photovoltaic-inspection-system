/**
 * @file turn_tune.c
 * @brief 航向校正外环PID参数在线调节与航向偏差对齐自测试底层调试模块。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "turn_tune.h"
#include "chassis.h"
#include "board_config.h"
#include "mpu6050.h"

static TurnTuneState_t g_turn_tune;

#define STEP_KP          0.1f
#define STEP_KI          0.05f
#define STEP_KD          0.1f
#define STEP_TGT_YAW     5.0f

#define MAX_KP           5.0f
#define MAX_KI           2.00f
#define MAX_KD           2.0f

/* Helper to clamp floats within [lo, hi] */
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


/* Push turn PID parameters to the chassis layer */
static void apply_pid(void)
{
    Chassis_SetTurnPID(g_turn_tune.kp, g_turn_tune.ki, g_turn_tune.kd);
}

/* Update target yaw or stop in the chassis layer */
static void apply_run_state(void)
{
    if (g_turn_tune.running)
    {
        Chassis_SetAbsoluteHeading(g_turn_tune.target_yaw, g_turn_tune.target_speed);
    }
    else
    {
        Chassis_Stop();
    }
}

/* ---- Public APIs ---- */

void TurnTune_Init(void)
{
    float kp, ki, kd;

    /* Pull current parameters from chassis so we start with real values */
    Chassis_GetTurnPID(&kp, &ki, &kd);
    g_turn_tune.kp = kp;
    g_turn_tune.ki = ki;
    g_turn_tune.kd = kd;
    
    g_turn_tune.target_yaw = Chassis_GetYaw();  /* default to calibrated Yaw */
    g_turn_tune.target_speed = g_auto_target_speed;
    g_turn_tune.selected = TURN_PARAM_KP;
    g_turn_tune.running = 0U;

    g_turn_tune.actual_yaw = Chassis_GetYaw();
    g_turn_tune.left_pwm = 0;
    g_turn_tune.right_pwm = 0;
    g_turn_tune.enc_left = 0;
    g_turn_tune.enc_right = 0;
}


void TurnTune_NextParam(void)
{
    g_turn_tune.selected = (TurnParamIndex_t)((g_turn_tune.selected + 1U) % TURN_PARAM_MAX);
}

void TurnTune_Increase(void)
{
    switch (g_turn_tune.selected)
    {
        case TURN_PARAM_KP:
            g_turn_tune.kp = clampf_t(g_turn_tune.kp + STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TURN_PARAM_KI:
            g_turn_tune.ki = clampf_t(g_turn_tune.ki + STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TURN_PARAM_KD:
            g_turn_tune.kd = clampf_t(g_turn_tune.kd + STEP_KD, 0.0f, MAX_KD);
            apply_pid();
            break;
        case TURN_PARAM_TGT_YAW:
            g_turn_tune.target_yaw = g_turn_tune.target_yaw + STEP_TGT_YAW;
            if (g_turn_tune.target_yaw > 180.0f) g_turn_tune.target_yaw -= 360.0f;
            if (g_turn_tune.running) apply_run_state();
            break;
        case TURN_PARAM_TGT_SPD:
            g_turn_tune.target_speed = clampi16_t(g_turn_tune.target_speed + 1, 3, 30);
            g_auto_target_speed = g_turn_tune.target_speed;
            if (g_turn_tune.running) apply_run_state();
            break;
        case TURN_PARAM_RUN_STOP:
            TurnTune_ToggleRun();
            break;
        default:
            break;
    }
}

void TurnTune_Decrease(void)
{
    switch (g_turn_tune.selected)
    {
        case TURN_PARAM_KP:
            g_turn_tune.kp = clampf_t(g_turn_tune.kp - STEP_KP, 0.0f, MAX_KP);
            apply_pid();
            break;
        case TURN_PARAM_KI:
            g_turn_tune.ki = clampf_t(g_turn_tune.ki - STEP_KI, 0.0f, MAX_KI);
            apply_pid();
            break;
        case TURN_PARAM_KD:
            g_turn_tune.kd = clampf_t(g_turn_tune.kd - STEP_KD, 0.0f, MAX_KD);
            apply_pid();
            break;
        case TURN_PARAM_TGT_YAW:
            g_turn_tune.target_yaw = g_turn_tune.target_yaw - STEP_TGT_YAW;
            if (g_turn_tune.target_yaw < -180.0f) g_turn_tune.target_yaw += 360.0f;
            if (g_turn_tune.running) apply_run_state();
            break;
        case TURN_PARAM_TGT_SPD:
            g_turn_tune.target_speed = clampi16_t(g_turn_tune.target_speed - 1, 3, 30);
            g_auto_target_speed = g_turn_tune.target_speed;
            if (g_turn_tune.running) apply_run_state();
            break;
        case TURN_PARAM_RUN_STOP:
            break;
        default:
            break;
    }
}


void TurnTune_ToggleRun(void)
{
    if (g_turn_tune.running)
    {
        g_turn_tune.running = 0U;
        Chassis_Stop();
    }
    else
    {
        if (Chassis_IsDriverPowered())
        {
            g_turn_tune.running = 1U;
            /* Lock to current target yaw when starting */
            apply_run_state();
        }
    }
}

void TurnTune_ForceStop(void)
{
    g_turn_tune.running = 0U;
    Chassis_Stop();
}

void TurnTune_RefreshData(void)
{
    const ChassisState_t *s = Chassis_GetState();

    /* Read back turn PID parameters in case they changed elsewhere */
    Chassis_GetTurnPID(&g_turn_tune.kp, &g_turn_tune.ki, &g_turn_tune.kd);

    g_turn_tune.actual_yaw = Chassis_GetYaw();
    g_turn_tune.left_pwm = s->pwm_left;
    g_turn_tune.right_pwm = s->pwm_right;
    g_turn_tune.enc_left = s->enc_left;
    g_turn_tune.enc_right = s->enc_right;

    if (s->mode != CHASSIS_MODE_HEADING && g_turn_tune.running)
    {
        g_turn_tune.running = 0U;
    }
}

const TurnTuneState_t *TurnTune_GetState(void)
{
    return &g_turn_tune;
}
