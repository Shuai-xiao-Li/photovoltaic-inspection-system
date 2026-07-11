/**
 * @file motor_driver.h
 * @brief 电机驱动头文件，声明电机的运动控制接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __MOTOR_DRIVER_H
#define __MOTOR_DRIVER_H

#include <stdint.h>

/*
 * Motor driver layer for the D153B/TB6612 dual motor driver.
 * This layer only knows about direction pins and PWM duty values.
 * It does not know about encoder feedback, PID, chassis speed, or LCD.
 */

void Motor_Init(void);
void Motor_EnableOutputs(void);
void Motor_DisableOutputsHiZ(void);
uint8_t Motor_OutputsEnabled(void);
void Motor_SetPWM(int16_t left_pwm, int16_t right_pwm);
void Motor_Stop(void);
void Motor_Brake(void);

#endif
