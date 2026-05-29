#include "debug_protocol.h"
#include "board_config.h"
#include "usart.h"
#include "chassis.h"
#include "display_ui.h"
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
    print_status();
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
    else if (strncmp(line, "vel", 3) == 0)
    {
        if (sscanf(line + 3, "%f %f", &a, &b) == 2)
        {
            Chassis_SetVelocity(a, b);
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
    else if (strncmp(line, "page", 4) == 0)
    {
        if (sscanf(line + 4, "%d", &l_i) == 1 && l_i >= 0 && l_i < UI_PAGE_MAX)
        {
            DisplayUI_SetPage((UI_Page_t)l_i);
            Debug_Printf("OK page %d\r\n", l_i);
        }
        else Debug_Printf("ERR page 0/1/2\r\n");
    }
    else if (strncmp(line, "stop", 4) == 0)
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
        print_help();
    }
}

void DebugProtocol_Init(void)
{
    /*
     * ALIENTEK's SYSTEM/usart/usart.c already starts USART1 interrupt receive
     * inside uart_init().  To avoid two receive engines fighting for USART1,
     * this Keil integration version reads completed lines from USART_RX_BUF and
     * USART_RX_STA in DebugProtocol_Task().
     */
    Debug_Printf("\r\nSTM32F103ZET6 tracked chassis LCD lower controller ready. Type help.\r\n");
    Debug_Printf("Recommended first command: test low\r\n");
}

void DebugProtocol_Task(void)
{
    char local_line[RX_LINE_MAX];
    uint16_t len;

    /*
     * ALIENTEK USART receive format:
     * - USART_RX_STA bit15 means one line has been received.
     * - lower 14 bits store the received byte count.
     * - line ending is CRLF.  Use serial assistant with "send new line" enabled.
     */
    if (USART_RX_STA & 0x8000U)
    {
        len = USART_RX_STA & 0x3FFFU;
        if (len >= RX_LINE_MAX) len = RX_LINE_MAX - 1U;

        memcpy(local_line, USART_RX_BUF, len);
        local_line[len] = '\0';

        USART_RX_STA = 0;
        memset(USART_RX_BUF, 0, USART_REC_LEN);

        parse_line(local_line);
    }
}

void DebugProtocol_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /*
     * Not used in the ALIENTEK Keil integration path.
     * This function is kept so the same header can still be used in CubeMX-style
     * projects if you later migrate.
     */
    (void)huart;
}
