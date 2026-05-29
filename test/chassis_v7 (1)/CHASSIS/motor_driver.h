#ifndef __MOTOR_DRIVER_H
#define __MOTOR_DRIVER_H

#include <stdint.h>

/*
 * Motor driver layer for the D153B/TB6612 dual motor driver.
 * This layer only knows about direction pins and PWM duty values.
 * It does not know about encoder feedback, PID, chassis speed, or LCD.
 */

void Motor_Init(void);
void Motor_EnableOutputs(void);
void Motor_DisableOutputsHiZ(void);
uint8_t Motor_OutputsEnabled(void);
void Motor_SetPWM(int16_t left_pwm, int16_t right_pwm);
void Motor_Stop(void);
void Motor_Brake(void);

#endif
