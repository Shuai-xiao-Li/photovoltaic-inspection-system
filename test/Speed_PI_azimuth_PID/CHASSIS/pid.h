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
