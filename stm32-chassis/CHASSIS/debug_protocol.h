/**
 * @file debug_protocol.h
 * @brief 调试协议包解析头文件，定义串口通信帧格式及协议接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __DEBUG_PROTOCOL_H
#define __DEBUG_PROTOCOL_H

#include "stm32f1xx_hal.h"

void DebugProtocol_Init(void);
void DebugProtocol_Task(void);
void DebugProtocol_RxCpltCallback(UART_HandleTypeDef *huart);
void Debug_Printf(const char *fmt, ...);

#endif
