#ifndef __CHASSIS_H
#define __CHASSIS_H

#include <stdint.h>
#include "pid.h"

/* Chassis running modes. */
typedef enum
{
    CHASSIS_MODE_STOP = 0,
    CHASSIS_MODE_OPEN_LOOP,
    CHASSIS_MODE_SPEED_COUNT,
    CHASSIS_MODE_VEL_MM,
    CHASSIS_MODE_HEADING
} ChassisMode_t;

/* Fault bit definitions.  fault_code can contain more than one bit. */
#define CHASSIS_FAULT_NONE         0x00U
#define CHASSIS_FAULT_ESTOP        0x01U
#define CHASSIS_FAULT_LOW_BATTERY  0x02U
#define CHASSIS_FAULT_CMD_TIMEOUT  0x04U
#define CHASSIS_FAULT_DRIVER_OFF   0x08U  /* non-latched display bit used only when needed */

/* Public state snapshot.  LCD and serial status read this structure.
 *
 * Naming rule:
 * - desired_* means the latest user command after software safety limiting.
 * - target_* means the ramped value currently used by PID.
 * - pwm_* means the ramped value currently written to the motor layer.
 */
typedef struct
{
    ChassisMode_t mode;

    int16_t enc_left;              /* signed feedback in the latest 10 ms */
    int16_t enc_right;
    int32_t enc_left_total;        /* diagnostic accumulated feedback since boot */
    int32_t enc_right_total;

    int16_t desired_left_count;
    int16_t desired_right_count;
    int16_t target_left_count;
    int16_t target_right_count;

    int16_t desired_pwm_left;
    int16_t desired_pwm_right;
    int16_t pwm_left;
    int16_t pwm_right;
    int16_t pi_correction_left;       /* PI part before speed feed-forward */
    int16_t pi_correction_right;
    int16_t speed_cmd_pwm_left;       /* feed-forward + PI before PWM ramp */
    int16_t speed_cmd_pwm_right;

    float cmd_v_mm_s;
    float cmd_w_mrad_s;
    float battery_v;

    uint32_t control_tick;
    uint32_t last_cmd_tick_ms;

    uint8_t fault_code;
    uint8_t command_limited;

    /* 1 means D153B VM is present according to PA5/ADC voltage sampling.
     * When 0, motor outputs are forced to high-impedance to prevent GPIO
     * back-feeding into the unpowered driver.
     */
    uint8_t driver_powered;
    uint8_t driver_blocked;
} ChassisState_t;

/* RAM block for ST-LINK live tuning.
 * PC tools find g_stlink_tune in OBJ/LCD.map, then read/write this block with
 * STM32_Programmer_CLI while the firmware keeps running.
 */
#define STLINK_TUNE_MAGIC          0x49505453UL  /* "STPI" little-endian */
#define STLINK_TUNE_VERSION        1UL

#define STLINK_TUNE_CMD_NONE       0UL
#define STLINK_TUNE_CMD_APPLY      1UL
#define STLINK_TUNE_CMD_RUN        2UL
#define STLINK_TUNE_CMD_STOP       3UL
#define STLINK_TUNE_CMD_CLEARFAULT 4UL

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t seq;
    uint32_t ack_seq;
    uint32_t command;
    uint32_t run;

    int32_t left_kp_x100;
    int32_t right_kp_x100;
    int32_t left_ki_x1000;
    int32_t right_ki_x1000;
    int32_t left_ff;
    int32_t right_ff;
    int32_t target_count;

    uint32_t tick;
    int32_t mode;
    int32_t left_target;
    int32_t left_actual;
    int32_t left_pwm;
    int32_t left_correction;
    int32_t left_cmd_pwm;
    int32_t right_target;
    int32_t right_actual;
    int32_t right_pwm;
    int32_t right_correction;
    int32_t right_cmd_pwm;
    int32_t left_total;
    int32_t right_total;
    int32_t battery_mv;
    uint32_t fault_code;
    uint32_t driver_powered;
} ChassisStlinkTuneBlock_t;

extern volatile ChassisStlinkTuneBlock_t g_stlink_tune;

void Chassis_Init(void);
void Chassis_ControlLoop10ms(void);
void Chassis_SetOpenLoopPWM(int16_t left_pwm, int16_t right_pwm);
void Chassis_SetSpeedCount(int16_t left_count_per_period, int16_t right_count_per_period);
void Chassis_SetVelocity(float v_mm_s, float w_mrad_s);
void Chassis_SetAbsoluteHeading(float target_yaw_degrees, int16_t forward_speed_count);
void Chassis_Stop(void);
void Chassis_EStop(void);
void Chassis_ClearFault(void);
void Chassis_SetPID(float kp, float ki, float kd);
void Chassis_SetLeftPID(float kp, float ki);
void Chassis_SetRightPID(float kp, float ki);
void Chassis_GetLeftPID(float *kp, float *ki);
void Chassis_GetRightPID(float *kp, float *ki);
void Chassis_SetMinPWM(int16_t left_min, int16_t right_min);
void Chassis_GetMinPWM(int16_t *left_min, int16_t *right_min);
void Chassis_SetTurnPID(float kp, float ki, float kd);
void Chassis_GetTurnPID(float *kp, float *ki, float *kd);

extern PID_Inc_t g_pid_turn;
extern float g_target_yaw;

uint8_t Chassis_IsDriverPowered(void);
void Chassis_RefreshDriverPower(void);
const ChassisState_t *Chassis_GetState(void);

void Chassis_SaveParamsToFlash(void);
void Chassis_LoadParamsFromFlash(void);
float Chassis_GetYaw(void);
void Chassis_ZeroYaw(void);
extern int16_t g_auto_target_speed;


#endif

