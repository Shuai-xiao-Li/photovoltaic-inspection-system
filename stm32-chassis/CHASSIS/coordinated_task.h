/**
 * @file coordinated_task.h
 * @brief 协调运行任务头文件，声明主调度任务入口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __COORDINATED_TASK_H
#define __COORDINATED_TASK_H

#include "sys.h"

typedef enum {
    COORD_IDLE = 0,
    COORD_FWD_1,
    COORD_STOP_1,
    COORD_LOOK_RT_UP,
    COORD_LOOK_RT,
    COORD_TURN_RT,
    COORD_FWD_2,
    COORD_STOP_2,
    COORD_LOOK_LT_DN,
    COORD_LOOK_LT,
    COORD_TURN_LT,
    COORD_FWD_3,
    COORD_DONE
} CoordState_t;

extern CoordState_t coord_state;

void CoordinatedTask_Start(void);
void CoordinatedTask_Run(void);
void CoordinatedTask_Stop(void);

#endif
