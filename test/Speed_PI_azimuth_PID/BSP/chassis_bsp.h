#ifndef __CHASSIS_BSP_H
#define __CHASSIS_BSP_H

#include "main.h"

/*
 * chassis_bsp.h
 *
 * This file supplies the peripheral handles and initialization functions that
 * are missing in the ALIENTEK TFTLCD example, because the ALIENTEK examples do
 * not use the CubeMX-style tim.c/adc.c/usart.c files.
 *
 * Add chassis_bsp.c to Keil and call ChassisBSP_Init() after delay_init(72).
 */

extern TIM_HandleTypeDef htim2;   /* right encoder: PA0/PA1 */
extern TIM_HandleTypeDef htim3;   /* motor PWM: PA6/PA7 */
extern TIM_HandleTypeDef htim6;   /* 10 ms control interrupt */
extern TIM_HandleTypeDef htim8;   /* left encoder: PC6/PC7 */
extern ADC_HandleTypeDef hadc1;   /* battery voltage: PA5/ADC1_IN5 */

void ChassisBSP_Init(void);

#endif
