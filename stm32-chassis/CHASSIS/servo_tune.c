/**
 * @file servo_tune.c
 * @brief 舵机调试控制接口，提供云台自校准、零偏及运动测试底层支撑。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "servo_tune.h"
#include "servo.h"

static ServoTuneState_t g_servo_tune;

void ServoTune_Init(void)
{
    g_servo_tune.selected = SERVO_PARAM_MODE;
}

void ServoTune_NextParam(void)
{
    g_servo_tune.selected = (ServoParamIndex_t)((g_servo_tune.selected + 1U) % SERVO_PARAM_MAX);
}

void ServoTune_Increase(void)
{
    switch (g_servo_tune.selected)
    {
        case SERVO_PARAM_MODE:
            servo_mode = !servo_mode;
            break;
        case SERVO_PARAM_S1_CENT:
            s1_center += 10;
            if (s1_center > 1700) s1_center = 1300;
            break;
        case SERVO_PARAM_S1_ANG:
            s1_angle += 5;
            if (s1_angle > 89) s1_angle = -83;
            break;
        case SERVO_PARAM_S2_CENT:
            s2_center += 10;
            if (s2_center > 1700) s2_center = 1300;
            break;
        case SERVO_PARAM_S2_ANG:
            s2_angle += 5;
            if (s2_angle > 32) s2_angle = -37;
            break;
        case SERVO_PARAM_S1_SAVE:
            s1_center = s1_center + (int)(s1_angle * 7.4f);
            s1_angle = 0;
            limit_error = 2; /* S1 CENT SAVED! */
            break;
        case SERVO_PARAM_S2_SAVE:
            s2_center = s2_center + (int)(s2_angle * 7.4f);
            s2_angle = 0;
            limit_error = 3; /* S2 CENT SAVED! */
            break;
        case SERVO_PARAM_SWEEP:
            Servo_SweepTest();
            limit_error = 4; /* SWEEP DONE */
            break;
        default:
            break;
    }
}

const ServoTuneState_t *ServoTune_GetState(void)
{
    return &g_servo_tune;
}
