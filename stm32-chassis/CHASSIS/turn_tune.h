/**
 * @file turn_tune.h
 * @brief 航向整定模块头文件，声明航向PID调参接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __TURN_TUNE_H
#define __TURN_TUNE_H

#include <stdint.h>

/* Index of each turn tuning parameter. */
typedef enum
{
    TURN_PARAM_KP = 0,
    TURN_PARAM_KI,
    TURN_PARAM_KD,
    TURN_PARAM_TGT_YAW,
    TURN_PARAM_TGT_SPD,
    TURN_PARAM_RUN_STOP,
    TURN_PARAM_MAX
} TurnParamIndex_t;

/* Snapshot of turn tuning state for display and adjustment. */
typedef struct
{
    float    kp;
    float    ki;
    float    kd;
    float    target_yaw;
    int16_t  target_speed;
    
    TurnParamIndex_t selected;  /* highlighted parameter index */

    uint8_t  running;           /* 1 = closed-loop running */

    /* Live diagnostics */
    float    actual_yaw;
    int16_t  left_pwm;
    int16_t  right_pwm;
    int16_t  enc_left;
    int16_t  enc_right;
} TurnTuneState_t;

void TurnTune_Init(void);
void TurnTune_NextParam(void);
void TurnTune_Increase(void);
void TurnTune_Decrease(void);
void TurnTune_ToggleRun(void);
void TurnTune_ForceStop(void);

const TurnTuneState_t *TurnTune_GetState(void);
void TurnTune_RefreshData(void);

#endif
