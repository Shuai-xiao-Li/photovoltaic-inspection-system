/**
 * @file coordinated_task.c
 * @brief 底盘各传感器与控制逻辑的协调运行任务，统一安排数据采集时序和底盘控制周期运行。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "coordinated_task.h"
#include "chassis.h"
#include "servo.h"
#include "delay.h"
#include "stm32f1xx_hal.h"
#include <math.h>

CoordState_t coord_state = COORD_IDLE;
static uint32_t coord_timer = 0;
static float absolute_base_yaw = 0.0f;
static float coord_initial_yaw = 0.0f;
static float coord_target_yaw = 0.0f;

void CoordinatedTask_Start(void)
{
    if (coord_state != COORD_IDLE) return;
    
    coord_state = COORD_FWD_1;
    coord_timer = HAL_GetTick();
    absolute_base_yaw = Chassis_GetYaw();
    coord_initial_yaw = absolute_base_yaw;
    
    // 设置为物理模式，准备控制舵机
    servo_mode = 1;
    s1_angle = 0;
    s2_angle = 0;
    
    Chassis_SetAbsoluteHeading(absolute_base_yaw, 100); // 假设100是合适的前进速度
}

void CoordinatedTask_Stop(void)
{
    coord_state = COORD_IDLE;
    Chassis_SetAbsoluteHeading(Chassis_GetYaw(), 0);
    s1_angle = 0;
    s2_angle = 0;
}

void CoordinatedTask_Run(void)
{
    uint32_t now;
    float current_yaw;
    float diff;

    if (coord_state == COORD_IDLE) return;
    
    now = HAL_GetTick();
    current_yaw = Chassis_GetYaw();

    switch (coord_state) {
        case COORD_FWD_1:
            if (now - coord_timer >= 5000) {
                Chassis_SetAbsoluteHeading(coord_initial_yaw, 0); // 停下
                coord_state = COORD_STOP_1;
                coord_timer = now;
            }
            break;
            
        case COORD_STOP_1:
            if (now - coord_timer >= 500) { // 等待车完全停稳
                // 舵机向右看30度，向上看15度
                s1_angle = -30;
                s2_angle = -15;
                coord_state = COORD_LOOK_RT_UP;
                coord_timer = now;
            }
            break;
            
        case COORD_LOOK_RT_UP:
            if (now - coord_timer >= 1000) { // 停一秒
                // 俯仰维度回正
                s2_angle = 0;
                coord_state = COORD_LOOK_RT;
                coord_timer = now;
            }
            break;
            
        case COORD_LOOK_RT:
            if (now - coord_timer >= 2000) { // 保持姿势停2秒
                // 车身开始向右原地旋转30度
                // 为了消除累计误差，我们基于最初的绝对航向计算目标！
                coord_initial_yaw = current_yaw; // 仅作为舵机转动进度的起点参考
                coord_target_yaw = absolute_base_yaw - 30.0f;
                if (coord_target_yaw < -180.0f) coord_target_yaw += 360.0f;
                
                Chassis_SetAbsoluteHeading(coord_target_yaw, 0);
                coord_state = COORD_TURN_RT;
                coord_timer = now;
            }
            break;
            
        case COORD_TURN_RT:
            // 舵机反方向（向左）以5度的步长慢慢回正
            // diff为负值，表示向右转了多少度
            diff = current_yaw - coord_initial_yaw;
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            
            // 计算补偿角度，步长5度
            {
                int compensate = (int)(-diff / 5.0f) * 5;
                int new_s1 = -30 + compensate;
                if (new_s1 > 0) new_s1 = 0;
                s1_angle = new_s1;
            }
            
            // 判断是否转完 (误差小于2度)
            diff = current_yaw - coord_target_yaw;
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            
            // 为了防止卡在1度误差，同时加一个超时保护 (5秒没转完强制进入下一步)
            if ((fabsf(diff) < 2.0f && (now - coord_timer) > 1000) || (now - coord_timer) > 5000) {
                s1_angle = 0;
                coord_initial_yaw = coord_target_yaw; // 更新为新方向
                Chassis_SetAbsoluteHeading(coord_initial_yaw, 100); // 朝着新方向直行
                coord_state = COORD_FWD_2;
                coord_timer = now;
            }
            break;
            
        case COORD_FWD_2:
            if (now - coord_timer >= 5000) { // 直行5秒
                Chassis_SetAbsoluteHeading(coord_initial_yaw, 0); // 停下
                coord_state = COORD_STOP_2;
                coord_timer = now;
            }
            break;
            
        case COORD_STOP_2:
            if (now - coord_timer >= 500) {
                // 舵机向左看30度，向下看15度
                s1_angle = 30;
                s2_angle = 15;
                coord_state = COORD_LOOK_LT_DN;
                coord_timer = now;
            }
            break;
            
        case COORD_LOOK_LT_DN:
            if (now - coord_timer >= 1000) { // 停一秒
                // 俯仰维度回正
                s2_angle = 0;
                coord_state = COORD_LOOK_LT;
                coord_timer = now;
            }
            break;
            
        case COORD_LOOK_LT:
            if (now - coord_timer >= 2000) { // 停2秒
                // 车身向左旋转30度回正
                // 为了绝对精确，目标直接设为最开始记录的 absolute_base_yaw！
                coord_initial_yaw = current_yaw;
                coord_target_yaw = absolute_base_yaw;
                
                Chassis_SetAbsoluteHeading(coord_target_yaw, 0);
                coord_state = COORD_TURN_LT;
                coord_timer = now;
            }
            break;
            
        case COORD_TURN_LT:
            // diff为正值，表示向左转了多少度
            diff = current_yaw - coord_initial_yaw;
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            
            {
                int compensate = (int)(-diff / 5.0f) * 5; // 向左转，补偿为负
                int new_s1 = 30 + compensate;
                if (new_s1 < 0) new_s1 = 0;
                s1_angle = new_s1;
            }
            
            diff = current_yaw - coord_target_yaw;
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            
            if ((fabsf(diff) < 2.0f && (now - coord_timer) > 1000) || (now - coord_timer) > 5000) {
                s1_angle = 0;
                coord_initial_yaw = coord_target_yaw;
                Chassis_SetAbsoluteHeading(coord_initial_yaw, 100); // 再次直行
                coord_state = COORD_FWD_3;
                coord_timer = now;
            }
            break;
            
        case COORD_FWD_3:
            if (now - coord_timer >= 5000) { // 前进5秒
                Chassis_SetAbsoluteHeading(coord_initial_yaw, 0); // 停下结束
                coord_state = COORD_DONE;
                coord_timer = now;
            }
            break;
            
        case COORD_DONE:
            // 可以等待一会然后回到IDLE，或者直接回到IDLE
            if (now - coord_timer >= 1000) {
                coord_state = COORD_IDLE;
            }
            break;
    }
}
