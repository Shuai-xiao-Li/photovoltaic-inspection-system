/**
 * @file debug_protocol.c
 * @brief 上位机/串口调试协议包解析器，解码下发的运动速度控制、PID参数实时调节以及故障清除指令。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

/*



 * debug_protocol_auto.c



 *



 * 鍩轰簬鍘熺増 debug_protocol.c 鐨勮嚜鍔ㄥ寲鐗堟湰銆



 * 鏂板炲唴瀹癸細



 *   1. #include "servo.h"



 *   2. sdelta <x> <y> 鍛戒护锛氫簯鍙板為噺鎺у埗 + 纭鎬х墿鐞嗛檺浣



 *   3. scenter 鍛戒护锛氫簯鍙颁竴閿褰掍腑



 *



 * 浣跨敤鏂规硶锛氬皢姝ゆ枃浠舵浛鎹㈠師宸ョ▼涓鐨 debug_protocol.c 鍗冲彲銆



 */







#include "debug_protocol.h"



#include "board_config.h"



#include "usart.h"



#include "chassis.h"



#include "display_ui.h"



#include "servo.h"



#include <stdio.h>



#include <string.h>



#include <stdarg.h>







#define RX_LINE_MAX 96







void Debug_Printf(const char *fmt, ...)



{



    char buf[300];



    va_list args;



    int len;







    va_start(args, fmt);



    len = vsnprintf(buf, sizeof(buf), fmt, args);



    va_end(args);







    if (len < 0) return;



    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;







    HAL_UART_Transmit(DEBUG_UART_HANDLE, (uint8_t *)buf, (uint16_t)len, 50);



}







static void print_help(void)



{



    Debug_Printf("\r\nSafe test commands:\r\n");



    Debug_Printf("  test low      safe forward test, %.0f mm/s\r\n", TEST_SPEED_LOW_MM_S);



    Debug_Printf("  test mid      safe forward test, %.0f mm/s\r\n", TEST_SPEED_MID_MM_S);



    Debug_Printf("  test high     upper bench test, %.0f mm/s\r\n", TEST_SPEED_HIGH_MM_S);



    Debug_Printf("  test left     slow rotate left, W=%.0f mrad/s\r\n", TEST_TURN_MRAD_S);



    Debug_Printf("  test right    slow rotate right\r\n");



    Debug_Printf("  stop          normal stop\r\n");



    Debug_Printf("  estop         emergency stop, then clear to recover\r\n");



    Debug_Printf("\r\nManual commands, all have software limits and ramps:\r\n");



    Debug_Printf("  raw L R       open-loop PWM, range +/- %d, min nonzero %d\r\n",



                 MOTOR_PWM_LIMIT, MOTOR_PWM_MIN_EFFECTIVE);



    Debug_Printf("  NOTE: motor commands require D153B ADC->PA5 voltage > %.1fV\r\n",
                 DRIVER_POWER_ON_VOLTAGE);



    Debug_Printf("  spd L R       closed-loop count/10ms, range +/- %d\r\n",



                 CHASSIS_MAX_COUNT_PER_10MS);



    Debug_Printf("  vel V W       V=mm/s +/-%.0f, W=mrad/s +/-%.0f\r\n",



                 CHASSIS_MAX_V_MM_S, CHASSIS_MAX_W_MRAD_S);



    Debug_Printf("  pid KP KI KD  e.g. pid 0.8 0.05 0\r\n");



    Debug_Printf("  sdelta S1 S2  gimbal incremental move\r\n");



    Debug_Printf("  scenter       gimbal center reset\r\n");



    Debug_Printf("  page N        LCD page 0=main, 1=encoder, 2=system\r\n");



    Debug_Printf("  status | clear | help\r\n");



}







static void print_status(void)



{



    const ChassisState_t *s = Chassis_GetState();







    Debug_Printf("mode=%d fault=0x%02X limited=%u drv=%u block=%u bat=%.2f encL=%d encR=%d totalL=%ld totalR=%ld desCntL=%d desCntR=%d tgtL=%d tgtR=%d desPwmL=%d desPwmR=%d pwmL=%d pwmR=%d corrL=%d corrR=%d cmdL=%d cmdR=%d v=%.1f w=%.1f page=%d\r\n",



                 (int)s->mode,



                 (unsigned int)s->fault_code,



                 (unsigned int)s->command_limited,



                 (unsigned int)s->driver_powered,



                 (unsigned int)s->driver_blocked,



                  s->battery_v,



                  s->enc_left,



                  s->enc_right,



                  (long)s->enc_left_total,



                  (long)s->enc_right_total,



                  s->desired_left_count,



                 s->desired_right_count,



                 s->target_left_count,



                 s->target_right_count,



                 s->desired_pwm_left,



                 s->desired_pwm_right,



                 s->pwm_left,



                 s->pwm_right,



                 s->pi_correction_left,



                 s->pi_correction_right,



                 s->speed_cmd_pwm_left,



                 s->speed_cmd_pwm_right,



                 s->cmd_v_mm_s,



                 s->cmd_w_mrad_s,



                 (int)DisplayUI_GetPage());



}







static void print_after_command(const char *name)



{



    const ChassisState_t *s = Chassis_GetState();







    Debug_Printf("OK %s", name);



    if (s->command_limited)



    {



        Debug_Printf("  [limited for safety]");



    }



    Debug_Printf("\r\n");



    // print_status(); // Commented out to reduce serial traffic & latency



}







static void parse_test_command(char *arg)



{



    while (*arg == ' ' || *arg == '\t') arg++;







    if (strncmp(arg, "low", 3) == 0)



    {



        Chassis_SetOpenLoopPWM(KEY_TEST_PWM_LOW, KEY_TEST_PWM_LOW);



        print_after_command("test low open pwm");



    }



    else if (strncmp(arg, "mid", 3) == 0)



    {



        Chassis_SetOpenLoopPWM(KEY_TEST_PWM_MID, KEY_TEST_PWM_MID);



        print_after_command("test mid open pwm");



    }



    else if (strncmp(arg, "high", 4) == 0)



    {



        Chassis_SetOpenLoopPWM(MOTOR_PWM_LIMIT, MOTOR_PWM_LIMIT);



        print_after_command("test high open pwm");



    }



    else if (strncmp(arg, "left", 4) == 0)



    {



        Chassis_SetOpenLoopPWM(-KEY_TEST_PWM_TURN, KEY_TEST_PWM_TURN);



        print_after_command("test left open pwm");



    }



    else if (strncmp(arg, "right", 5) == 0)



    {



        Chassis_SetOpenLoopPWM(KEY_TEST_PWM_TURN, -KEY_TEST_PWM_TURN);



        print_after_command("test right open pwm");



    }



    else



    {



        Debug_Printf("ERR test low/mid/high/left/right\r\n");



    }



}







static void parse_line(char *line)



{



    int l_i = 0;



    int r_i = 0;



    float a = 0.0f;



    float b = 0.0f;



    float c = 0.0f;







    while (*line == ' ' || *line == '\t') line++;







    if (strncmp(line, "test", 4) == 0)



    {



        parse_test_command(line + 4);



    }



    else if (strncmp(line, "raw", 3) == 0)



    {



        if (sscanf(line + 3, "%d %d", &l_i, &r_i) == 2)



        {



            Chassis_SetOpenLoopPWM((int16_t)l_i, (int16_t)r_i);



            print_after_command("raw");



        }



        else Debug_Printf("ERR raw L R\r\n");



    }



    else if (strncmp(line, "spd", 3) == 0)



    {



        if (sscanf(line + 3, "%d %d", &l_i, &r_i) == 2)



        {



            Chassis_SetSpeedCount((int16_t)l_i, (int16_t)r_i);



            print_after_command("spd");



        }



        else Debug_Printf("ERR spd L R\r\n");



    }



    else if (strncmp(line, "vel", 3) == 0 || strncmp(line, "v ", 2) == 0)



    {



        char *args = (strncmp(line, "vel", 3) == 0) ? line + 3 : line + 2;



        if (sscanf(args, "%d %d", &l_i, &r_i) == 2)



        {



            Chassis_SetVelocity((float)l_i, (float)r_i);



            print_after_command("vel");



        }



        else Debug_Printf("ERR vel V W\r\n");



    }



    else if (strncmp(line, "pid", 3) == 0)



    {



        if (sscanf(line + 3, "%f %f %f", &a, &b, &c) == 3)



        {



            Chassis_SetPID(a, b, c);



            Debug_Printf("OK pid %.3f %.3f %.3f\r\n", a, b, c);



        }



        else Debug_Printf("ERR pid KP KI KD\r\n");



    }



    /* 鈽呪槄鈽 鏂板烇細浜戝彴澧為噺鎺у埗鎸囦护 鈽呪槄鈽 */



else if (strncmp(line, "sdelta", 6) == 0 || strncmp(line, "s ", 2) == 0)



    {



        if (sscanf((strncmp(line, "sdelta", 6) == 0 ? line + 6 : line + 2), "%d %d", &l_i, &r_i) == 2)



        {



            servo_mode = 1;



            s1_angle += l_i;



            s2_angle += r_i;







            /* 线性物理限位保护，防止烧毁舵机 */



            if (s1_angle < -83) s1_angle = -83;



            if (s1_angle > 89) s1_angle = 89;



            if (s2_angle < -37) s2_angle = -37;



            if (s2_angle > 32) s2_angle = 32;







            Debug_Printf("OK sdelta %d %d\r\n", s1_angle, s2_angle);



        }



        else Debug_Printf("ERR sdelta DS1 DS2\r\n");



    }



    else if (strncmp(line, "scenter", 7) == 0 || strcmp(line, "c") == 0)



    {



        servo_mode = 1;



        s1_angle = 0;



        s2_angle = 0;



        Debug_Printf("OK scenter\r\n");



    }



    else if (strncmp(line, "page", 4) == 0)



    {



        if (sscanf(line + 4, "%d", &l_i) == 1 && l_i >= 0 && l_i < UI_PAGE_MAX)



        {



            DisplayUI_SetPage((UI_Page_t)l_i);



            Debug_Printf("OK page %d\r\n", l_i);



        }



        else Debug_Printf("ERR page 0/1/2\r\n");



    }



    else if (strncmp(line, "stop", 4) == 0 || strcmp(line, "p") == 0)



    {



        Chassis_Stop();



        Debug_Printf("OK stop\r\n");



    }



    else if (strncmp(line, "estop", 5) == 0)



    {



        Chassis_EStop();



        Debug_Printf("OK estop\r\n");



    }



    else if (strncmp(line, "clear", 5) == 0)



    {



        Chassis_ClearFault();



        Debug_Printf("OK clear\r\n");



    }



    else if (strncmp(line, "status", 6) == 0)



    {



        print_status();



    }



    else if (strncmp(line, "help", 4) == 0 || strncmp(line, "?", 1) == 0)



    {



        print_help();



    }



    else if (line[0] != '\0')



    {



        Debug_Printf("ERR unknown: %s\r\n", line);



        // print_help(); // 不再自动打印完整的帮助列表，防止高频报错堵塞串口



    }



}







void DebugProtocol_Init(void)



{



    Debug_Printf("\r\nSTM32F103ZET6 tracked chassis LCD lower controller ready. Type help.\r\n");



    Debug_Printf("Recommended first command: test low\r\n");



}







void DebugProtocol_Task(void)



{



    static char parse_buf[RX_LINE_MAX];



    static uint16_t parse_len = 0;



    u8 ch;







    // 从串口环形接收缓冲区中提取所有可用字节



    while (USART_Get_Char(&ch))



    {



        if (ch == '\r') continue; // 忽略回车



        if (ch == '\n')



        {



            if (parse_len > 0)



            {



                parse_buf[parse_len] = '\0';



                parse_line(parse_buf);



                parse_len = 0;



            }



        }



        else if (ch == '\b' || ch == 0x7F) // 支持退格键和删除键，方便串口助手手动输入时删除错字



        {



            if (parse_len > 0)



            {



                parse_len--;



            }



        }



        else



        {



            if (parse_len < RX_LINE_MAX - 1)



            {



                parse_buf[parse_len++] = (char)ch;



            }



            else



            {



                parse_len = 0; // 溢出重置



            }



        }



    }



}







void DebugProtocol_RxCpltCallback(UART_HandleTypeDef *huart)



{



    (void)huart;



}



