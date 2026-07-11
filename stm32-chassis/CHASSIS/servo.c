/**
 * @file servo.c
 * @brief 摄像头云台俯仰/水平双舵机控制驱动，基于PWM脉冲占空比精确控制舵机角度。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "servo.h"
#include "chassis_bsp.h"
#include "delay.h"

int servo_mode = 0;      /* 0: VIRTUAL ; 1: REAL */
int s1_center = 1511;    /* Center */
int s1_angle = 0;        /* -83 ~ +89 */
int s2_center = 1520;    /* Center */
int s2_angle = 0;        /* -37 ~ +32 */
int limit_error = 0;

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_MOE_ENABLE(&htim1); /* Force MOE enable for Advanced Timer TIM1 */
    
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1511);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 1520);
}

void Servo_SweepTest(void)
{
    /* 手动触发的自检扫视，提示舵机已激活并正常工作 */
    int i;
    
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1511);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 1520);
    delay_ms(500); // 稳定中位

    /* S1 水平扫视测试 (左右摇头) */
    for (i = 1511; i <= 1800; i += 5) { __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, i); delay_ms(5); }
    for (i = 1800; i >= 1200; i -= 5) { __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, i); delay_ms(5); }
    for (i = 1200; i <= 1511; i += 5) { __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, i); delay_ms(5); }

    /* S2 垂直扫视测试 (上下点头) */
    for (i = 1520; i <= 1700; i += 5) { __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, i); delay_ms(5); }
    for (i = 1700; i >= 1350; i -= 5) { __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, i); delay_ms(5); }
    for (i = 1350; i <= 1520; i += 5) { __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, i); delay_ms(5); }
    
    delay_ms(200); // 自检完成
    
    /* 恢复到当前变量对应的状态 */
    s1_angle = 0;
    s2_angle = 0;
}

void Servo_ControlLoop(void)
{
    int s1_pulse = 1500;
    int s2_pulse = 1500;

    if (limit_error == 1) limit_error = 0;

    /* Restrict angles to the safe ranges given */
    if (s1_angle > 89)  { s1_angle = 89;  limit_error = 1; }
    if (s1_angle < -83) { s1_angle = -83; limit_error = 1; }

    if (s2_angle > 32)  { s2_angle = 32;  limit_error = 1; }
    if (s2_angle < -37) { s2_angle = -37; limit_error = 1; }

    s1_pulse = s1_center + (int)(s1_angle * 7.4f);
    s2_pulse = s2_center + (int)(s2_angle * 7.4f);

    /* Strict hardware locks! NEVER exceed these pulses */
    if (s1_pulse > 2170) s1_pulse = 2170;
    if (s1_pulse < 900)  s1_pulse = 900;
    if (s2_pulse > 1760) s2_pulse = 1760;
    if (s2_pulse < 1250) s2_pulse = 1250;

    if (servo_mode == 1)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, s1_pulse);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, s2_pulse);
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, s1_center);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, s2_center);
    }
}
