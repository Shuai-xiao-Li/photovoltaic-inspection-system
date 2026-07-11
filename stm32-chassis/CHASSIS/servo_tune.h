/**
 * @file servo_tune.h
 * @brief 舵机调试接口头文件。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __SERVO_TUNE_H
#define __SERVO_TUNE_H

#include <stdint.h>

typedef enum
{
    SERVO_PARAM_MODE = 0,
    SERVO_PARAM_S1_CENT,
    SERVO_PARAM_S1_ANG,
    SERVO_PARAM_S2_CENT,
    SERVO_PARAM_S2_ANG,
    SERVO_PARAM_S1_SAVE,
    SERVO_PARAM_S2_SAVE,
    SERVO_PARAM_SWEEP,
    SERVO_PARAM_MAX
} ServoParamIndex_t;

typedef struct
{
    ServoParamIndex_t selected;
} ServoTuneState_t;

void ServoTune_Init(void);
void ServoTune_NextParam(void);
void ServoTune_Increase(void);

const ServoTuneState_t *ServoTune_GetState(void);

#endif
