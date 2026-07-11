/**
 * @file encoder_driver.h
 * @brief 编码器驱动头文件，提供编码器读数及清除计数接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __ENCODER_DRIVER_H
#define __ENCODER_DRIVER_H

#include <stdint.h>

/* Timer encoder reader.  Each read returns the delta count since last read.
 * The chassis controller calls these functions once every 10 ms.
 */

void Encoder_Init(void);
void Encoder_Reset(void);
int16_t Encoder_ReadLeftDelta(void);
int16_t Encoder_ReadRightDelta(void);

/* Global diagnostic variables for encoder start status */
extern uint8_t g_enc_l_status;
extern uint8_t g_enc_r_status;

#endif
