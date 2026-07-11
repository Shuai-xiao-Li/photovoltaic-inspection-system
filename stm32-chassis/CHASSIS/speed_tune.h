/**
 * @file speed_tune.h
 * @brief 速度环整定模块头文件，声明速度PI调参接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __SPEED_TUNE_H
#define __SPEED_TUNE_H

#include <stdint.h>

/*
 * speed_tune.h
 *
 * Speed PI tuning module for the tracked chassis.
 * Manages 7 tunable parameters and a RUN/STOP toggle.
 * All parameter changes are applied to the chassis layer immediately.
 *
 * This module is driven by key_control.c when the LCD is on UI_PAGE_TUNE,
 * and rendered by display_ui.c.
 */

/* Index of each tunable parameter. */
typedef enum
{
    TUNE_PARAM_L_KP = 0,
    TUNE_PARAM_L_KI,
    TUNE_PARAM_R_KP,
    TUNE_PARAM_R_KI,
    TUNE_PARAM_L_MINPWM,
    TUNE_PARAM_R_MINPWM,
    TUNE_PARAM_TGT_SPD,
    TUNE_PARAM_RUN_STOP,    /* not a number; KEY1 short toggles run/stop */
    TUNE_PARAM_MAX
} TuneParamIndex_t;

/* Snapshot of tuning state, read by display_ui for rendering. */
typedef struct
{
    /* Tunable parameters */
    float    left_kp;
    float    left_ki;
    float    right_kp;
    float    right_ki;
    int16_t  left_min_pwm;
    int16_t  right_min_pwm;
    int16_t  target_speed;      /* encoder count per 10 ms */

    /* UI state */
    TuneParamIndex_t selected;  /* currently highlighted parameter */
    uint8_t  running;           /* 1 = motor running at target_speed */

    /* Real-time motor feedback (copied from ChassisState each refresh) */
    int16_t  left_target;
    int16_t  left_actual;
    int16_t  left_pwm;
    int16_t  left_correction;
    int16_t  left_command_pwm;
    int16_t  right_target;
    int16_t  right_actual;
    int16_t  right_pwm;
    int16_t  right_correction;
    int16_t  right_command_pwm;
} SpeedTuneState_t;

void SpeedTune_Init(void);

/* Key actions called by key_control.c when on TUNE page */
void SpeedTune_NextParam(void);
void SpeedTune_Increase(void);
void SpeedTune_Decrease(void);
void SpeedTune_ToggleRun(void);
void SpeedTune_ForceStop(void);

/* Called by display_ui.c to get current state for rendering */
const SpeedTuneState_t *SpeedTune_GetState(void);

/* Called by display_ui.c each refresh to copy live motor data */
void SpeedTune_RefreshMotorData(void);

#endif
