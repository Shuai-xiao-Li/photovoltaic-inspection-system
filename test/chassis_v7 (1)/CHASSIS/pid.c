#include "pid.h"

static float clampf(float x, float min_v, float max_v)
{
    if (x > max_v) return max_v;
    if (x < min_v) return min_v;
    return x;
}

void PID_IncInit(PID_Inc_t *pid, float kp, float ki, float kd, float out_min, float out_max)
{
    if (!pid) return;
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->output = 0.0f;
    pid->last_error = 0.0f;
    pid->last_last_error = 0.0f;
    pid->output_min = out_min;
    pid->output_max = out_max;
}

void PID_IncSetParam(PID_Inc_t *pid, float kp, float ki, float kd)
{
    if (!pid) return;
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void PID_IncReset(PID_Inc_t *pid)
{
    if (!pid) return;
    pid->output = 0.0f;
    pid->last_error = 0.0f;
    pid->last_last_error = 0.0f;
}

float PID_IncCalc(PID_Inc_t *pid, float target, float feedback)
{
    float error;
    float delta;

    if (!pid) return 0.0f;

    error = target - feedback;

    /* Incremental PID:
     * output(k) = output(k-1)
     *           + Kp*(e(k)-e(k-1))
     *           + Ki*e(k)
     *           + Kd*(e(k)-2e(k-1)+e(k-2))
     * For the first stage, Kd can stay 0.
     */
    delta = pid->kp * (error - pid->last_error)
          + pid->ki * error
          + pid->kd * (error - 2.0f * pid->last_error + pid->last_last_error);

    pid->output += delta;
    pid->output = clampf(pid->output, pid->output_min, pid->output_max);

    pid->last_last_error = pid->last_error;
    pid->last_error = error;

    return pid->output;
}
