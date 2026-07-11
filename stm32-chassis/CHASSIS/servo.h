/**
 * @file servo.h
 * @brief 云台舵机驱动头文件，提供云台占空比设置声明。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __SERVO_H
#define __SERVO_H

#include "main.h"

#define SERVO_SAFE_MIN  600
#define SERVO_SAFE_MAX  2400

extern int servo_mode;      /* 0: VIRTUAL, 1: REAL */
extern int s1_center;
extern int s1_angle;
extern int s2_center;
extern int s2_angle;
extern int limit_error;

void Servo_Init(void);
void Servo_ControlLoop(void);
void Servo_SweepTest(void);

#endif
