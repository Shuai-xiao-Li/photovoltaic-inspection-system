/**
 * @file pid.h
 * @brief 通用PID控制器头文件，定义PID结构体和更新计算接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __PID_H
#define __PID_H

typedef struct
{
    float kp;
    float ki;
    float kd;
    float output;
    float last_error;
    float last_last_error;
    float output_min;
    float output_max;
} PID_Inc_t;

void PID_IncInit(PID_Inc_t *pid, float kp, float ki, float kd, float out_min, float out_max);
void PID_IncSetParam(PID_Inc_t *pid, float kp, float ki, float kd);
void PID_IncReset(PID_Inc_t *pid);
float PID_IncCalc(PID_Inc_t *pid, float target, float feedback);

#endif
