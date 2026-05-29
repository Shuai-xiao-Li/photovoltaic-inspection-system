#ifndef __DEBUG_PROTOCOL_H
#define __DEBUG_PROTOCOL_H

#include "stm32f1xx_hal.h"

void DebugProtocol_Init(void);
void DebugProtocol_Task(void);
void DebugProtocol_RxCpltCallback(UART_HandleTypeDef *huart);
void Debug_Printf(const char *fmt, ...);

#endif
