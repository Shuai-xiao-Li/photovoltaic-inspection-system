#include "display_ui.h"
#include "board_config.h"
#include "chassis.h"
#include "key_control.h"
#include "speed_tune.h"
#include "turn_tune.h"
#include "encoder_driver.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>

extern float Yaw;
extern float g_target_yaw;

/*
 * LCD dashboard for the 2.8-inch ALIENTEK TFTLCD.
 *
 * Design rules:
 * 1. No touch support is used.
 * 2. LCD drawing is done in main loop by DisplayUI_Task(), never in TIM6 ISR.
 * 3. The UI uses landscape mode so the 2.8-inch LCD becomes 320x240.
 * 4. Only the value area is refreshed each time to reduce flicker.
 */

static UI_Page_t g_page = UI_PAGE_MAIN;
static UI_Page_t g_last_page = UI_PAGE_MAX;
static uint32_t g_last_refresh_ms = 0U;
#if LCD_UI_AUTO_PAGE_MS > 0
static uint32_t g_last_auto_page_ms = 0U;
#endif

/*
 * Value-line cache.
 *
 * The first version cleared every dynamic line with LCD_Fill() and then printed
 * new text every 100 ms.  That is simple, but on the 2.8-inch parallel TFTLCD
 * it can look like high-frequency flicker.  This version avoids clearing lines
 * on every refresh.  Instead, strings are padded with spaces and redrawn only
 * when their text really changes.
 */
#define UI_CACHE_LINES 16U
#define UI_CACHE_TEXT_LEN 48U
static char g_line_cache[UI_CACHE_LINES][UI_CACHE_TEXT_LEN];

static const char *mode_name(ChassisMode_t mode)
{
    switch (mode)
    {
        case CHASSIS_MODE_STOP:        return "STOP";
        case CHASSIS_MODE_OPEN_LOOP:   return "OPEN";
        case CHASSIS_MODE_SPEED_COUNT: return "COUNT";
        case CHASSIS_MODE_VEL_MM:      return "VEL";
        default:                       return "UNKNOWN";
    }
}

static const char *fault_name(uint8_t fault)
{
    if (fault == CHASSIS_FAULT_NONE) return "NONE";
    if (fault & CHASSIS_FAULT_ESTOP) return "ESTOP";
    if (fault & CHASSIS_FAULT_LOW_BATTERY) return "LOW_BAT";
    if (fault & CHASSIS_FAULT_CMD_TIMEOUT) return "TIMEOUT";
    if (fault & CHASSIS_FAULT_DRIVER_OFF) return "DRV_OFF";
    return "FAULT";
}

static void ui_clear_cache(void)
{
    uint8_t i;
    for (i = 0U; i < UI_CACHE_LINES; i++)
    {
        g_line_cache[i][0] = '\0';
    }
}

static uint8_t ui_cache_index(uint16_t y)
{
    return (uint8_t)((y / 16U) % UI_CACHE_LINES);
}

static void ui_print(uint16_t x, uint16_t y, const char *s, uint16_t color)
{
    POINT_COLOR = color;
    BACK_COLOR = BLACK;
    LCD_ShowString(x, y, lcddev.width - x, 16, 16, (uint8_t *)s);
}

static void ui_value_line(uint16_t y, const char *s, uint16_t color)
{
    uint8_t index = ui_cache_index(y);
    char padded[UI_CACHE_TEXT_LEN];

    /* 39 characters roughly fill a 320-pixel line with 8x16 font.
     * Padding with spaces overwrites old longer values without clearing the
     * whole line, so the visual flicker is much lower.
     */
    snprintf(padded, sizeof(padded), "%-39s", s);

    if (strncmp(g_line_cache[index], padded, UI_CACHE_TEXT_LEN) == 0)
    {
        return;
    }

    strncpy(g_line_cache[index], padded, UI_CACHE_TEXT_LEN - 1U);
    g_line_cache[index][UI_CACHE_TEXT_LEN - 1U] = '\0';
    ui_print(4, y, padded, color);
}

static void draw_header(const char *title)
{
    LCD_Fill(0, 0, lcddev.width - 1, 23, DARKBLUE);
    POINT_COLOR = WHITE;
    BACK_COLOR = DARKBLUE;
    LCD_ShowString(4, 4, lcddev.width - 8, 16, 16, (uint8_t *)title);

    BACK_COLOR = BLACK;
    POINT_COLOR = GRAY;
    LCD_DrawLine(0, 24, lcddev.width - 1, 24);
}

static void draw_static_layout(void)
{
    LCD_Clear(BLACK);
    ui_clear_cache();

    switch (g_page)
    {
        case UI_PAGE_MAIN:
            draw_header("Tracked Chassis - Main");
            ui_print(4, 30,  "MODE:", GRAY);
            ui_print(4, 50,  "BAT :", GRAY);
            ui_print(4, 70,  "CMD :", GRAY);
            ui_print(4, 85,  "YAW :", GRAY);
            ui_print(4, 100, "LEFT:", GRAY);
            ui_print(4, 120, "RIGHT:", GRAY);
            ui_print(4, 150, "FAULT:", GRAY);
            ui_print(4, 220, "K0 select  K1 run  Hold K1 ESTOP", LGRAY);
            break;

        case UI_PAGE_ENCODER:
            draw_header("Tracked Chassis - Encoder");
            ui_print(4, 30,  "DELTA=LAST 10MS   TOTAL=SINCE BOOT", CYAN);
            ui_print(4, 220, "Forward spin: both TOTAL values increase", LGRAY);
            break;

        case UI_PAGE_SYSTEM:
            draw_header("Tracked Chassis - System");
            ui_print(4, 30,  "PWM pins : PA6/PA7", CYAN);
            ui_print(4, 50,  "LCD     : 2.8 inch, no touch", CYAN);
            ui_print(4, 65,  "ENC: PC6/7(T8) PA0/1(T2), P7 OFF", CYAN);
            ui_print(4, 80,  "CTRL_T  : 10 ms", GRAY);
            ui_print(4, 100, "TRACK_W : 221 mm", GRAY);
            ui_print(4, 120, "SPROCKET: 45 mm", GRAY);
            ui_print(4, 150, "CPR_OUT : 1560", GRAY);
            ui_print(4, 170, "PWM_MAX : 3800  RAMP: 60/10ms", GRAY);
            ui_print(4, 190, "DRV check: D153B ADC -> PA5", LGRAY);
            break;

        case UI_PAGE_TUNE:
            draw_header("Speed PI Tuning");
            ui_print(4, 96, "---- LEFT ----  ---- RIGHT ---", GRAY);
            ui_print(4, 184, "DEF KP60 KI0 FF L/R:1500/1400", LGRAY);
            ui_print(4, 200, "BOOST L/R:2100/2000 250ms R300", LGRAY);
            ui_print(4, 220, "K0:sel K1:+ K1L:-/ESTOP", LGRAY);
            break;

        case UI_PAGE_TURN_TUNE:
            draw_header("Turn PID Tuning");
            ui_print(4, 96, "---- YAW ----  --- MOTOR PWM --", GRAY);
            ui_print(4, 184, "KP Step:0.1  KI:0.05  KD:0.1", LGRAY);
            ui_print(4, 200, "Lock Yaw: TEST AUTO base speed", LGRAY);
            ui_print(4, 220, "K0:sel K1:+ K1L:-/ESTOP", LGRAY);
            break;

        default:
            break;
    }
}

/* Cursor character for selected parameter. */
static char tune_cursor(TuneParamIndex_t sel, TuneParamIndex_t me)
{
    return (sel == me) ? '>' : ' ';
}

static void update_tune_page(void)
{
    const SpeedTuneState_t *t;
    char buf[64];
    TuneParamIndex_t sel;

    SpeedTune_RefreshMotorData();
    t = SpeedTune_GetState();
    sel = t->selected;

    /* Row 1: L_KP / R_KP */
    snprintf(buf, sizeof(buf), "%cL_KP=%5.2f  %cR_KP=%5.2f",
             tune_cursor(sel, TUNE_PARAM_L_KP), t->left_kp,
             tune_cursor(sel, TUNE_PARAM_R_KP), t->right_kp);
    ui_value_line(28, buf, WHITE);

    /* Row 2: L_KI / R_KI */
    snprintf(buf, sizeof(buf), "%cL_KI=%5.3f  %cR_KI=%5.3f",
             tune_cursor(sel, TUNE_PARAM_L_KI), t->left_ki,
             tune_cursor(sel, TUNE_PARAM_R_KI), t->right_ki);
    ui_value_line(44, buf, WHITE);

    /* Row 3: per-side speed-loop feed-forward / open-loop minimum PWM. */
    snprintf(buf, sizeof(buf), "%cL_FF =%4d   %cR_FF =%4d",
             tune_cursor(sel, TUNE_PARAM_L_MINPWM), t->left_min_pwm,
             tune_cursor(sel, TUNE_PARAM_R_MINPWM), t->right_min_pwm);
    ui_value_line(60, buf, WHITE);

    /* Row 4: TGT_SPD / RUN-STOP */
    snprintf(buf, sizeof(buf), "%cTGT=%3d     %c[%s]",
             tune_cursor(sel, TUNE_PARAM_TGT_SPD), t->target_speed,
             tune_cursor(sel, TUNE_PARAM_RUN_STOP),
             t->running ? "RUN " : "STOP");
    ui_value_line(76, buf, t->running ? GREEN : WHITE);

    /* Separator at Y=96 drawn in static layout */

    /* Motor feedback and PI correction: LEFT */
    snprintf(buf, sizeof(buf), "L T:%3d A:%3d PWM:%4d C:%4d",
             t->left_target, t->left_actual, t->left_pwm, t->left_correction);
    ui_value_line(112, buf, YELLOW);

    /* Motor feedback and PI correction: RIGHT */
    snprintf(buf, sizeof(buf), "R T:%3d A:%3d PWM:%4d C:%4d",
             t->right_target, t->right_actual, t->right_pwm, t->right_correction);
    ui_value_line(128, buf, YELLOW);

    snprintf(buf, sizeof(buf), "CMD L/R:%4d / %4d",
             t->left_command_pwm, t->right_command_pwm);
    ui_value_line(144, buf, CYAN);

    /* Status line */
    snprintf(buf, sizeof(buf), "STATUS: %s  DRV:%s",
             t->running ? "RUNNING" : "STOPPED",
             Chassis_IsDriverPowered() ? "ON" : "OFF");
    ui_value_line(160, buf, t->running ? GREEN : LGRAY);
}

static char turn_tune_cursor(TurnParamIndex_t sel, TurnParamIndex_t me)
{
    return (sel == me) ? '>' : ' ';
}

static void update_turn_tune_page(void)
{
    const TurnTuneState_t *t;
    char buf[64];
    TurnParamIndex_t sel;

    TurnTune_RefreshData();
    t = TurnTune_GetState();
    sel = t->selected;

    /* Row 1: KP / KI */
    snprintf(buf, sizeof(buf), "%cKP =%5.2f   %cKI =%5.3f",
             turn_tune_cursor(sel, TURN_PARAM_KP), t->kp,
             turn_tune_cursor(sel, TURN_PARAM_KI), t->ki);
    ui_value_line(28, buf, WHITE);

    /* Row 2: KD / TGT_YAW */
    snprintf(buf, sizeof(buf), "%cKD =%5.2f   %cTGT=%6.1f",
             turn_tune_cursor(sel, TURN_PARAM_KD), t->kd,
             turn_tune_cursor(sel, TURN_PARAM_TGT_YAW), t->target_yaw);
    ui_value_line(44, buf, WHITE);

    /* Row 3: TGT_SPD / RUN_STOP */
    snprintf(buf, sizeof(buf), "%cSPD=%3d      %c[%s]",
             turn_tune_cursor(sel, TURN_PARAM_TGT_SPD), t->target_speed,
             turn_tune_cursor(sel, TURN_PARAM_RUN_STOP),
             t->running ? "RUN " : "STOP");
    ui_value_line(60, buf, t->running ? GREEN : WHITE);

    /* Real-time feedback */
    snprintf(buf, sizeof(buf), "ACT YAW : %6.1f  TGT YAW: %6.1f",
             t->actual_yaw, t->target_yaw);
    ui_value_line(112, buf, YELLOW);

    snprintf(buf, sizeof(buf), "ENC L/R : %4d / %4d",
             t->enc_left, t->enc_right);
    ui_value_line(128, buf, CYAN);

    snprintf(buf, sizeof(buf), "PWM L/R : %4d / %4d",
             t->left_pwm, t->right_pwm);
    ui_value_line(144, buf, CYAN);

    /* Status line */
    snprintf(buf, sizeof(buf), "STATUS: %s  DRV:%s",
             t->running ? "RUNNING" : "STOPPED",
             Chassis_IsDriverPowered() ? "ON" : "OFF");
    ui_value_line(160, buf, t->running ? GREEN : LGRAY);
}

static void update_main_page(const ChassisState_t *s)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "MODE: %-6s  TICK:%lu", mode_name(s->mode), (unsigned long)s->control_tick);
    ui_value_line(30, buf, WHITE);

    snprintf(buf, sizeof(buf), "BAT : %5.2f V  DRV:%s", s->battery_v, s->driver_powered ? "ON" : "OFF");
    ui_value_line(50, buf, s->driver_powered ? GREEN : RED);

    snprintf(buf, sizeof(buf), "CMD : V=%6.1f W=%6.1f", s->cmd_v_mm_s, s->cmd_w_mrad_s);
    ui_value_line(70, buf, WHITE);

    snprintf(buf, sizeof(buf), "YAW : ACT=%6.1f  TGT=%6.1f", Chassis_GetYaw(), g_target_yaw);

    ui_value_line(85, buf, CYAN);

    snprintf(buf, sizeof(buf), "LEFT : D=%4d T=%4d F=%4d P=%4d",
             s->desired_left_count, s->target_left_count, s->enc_left, s->pwm_left);
    ui_value_line(100, buf, YELLOW);

    snprintf(buf, sizeof(buf), "RIGHT: D=%4d T=%4d F=%4d P=%4d",
             s->desired_right_count, s->target_right_count, s->enc_right, s->pwm_right);
    ui_value_line(120, buf, YELLOW);

    snprintf(buf, sizeof(buf), "FAULT: %s  LIM:%s BLK:%s",
             fault_name(s->fault_code),
             s->command_limited ? "YES" : "NO",
             s->driver_blocked ? "YES" : "NO");
    ui_value_line(150, buf, (s->fault_code || s->command_limited || s->driver_blocked) ? RED : GREEN);

    snprintf(buf, sizeof(buf), "SELECT: %s", KeyControl_GetActionName());
    ui_value_line(180, buf, CYAN);

    snprintf(buf, sizeof(buf), "EVENT : %s", KeyControl_GetLastEvent());
    ui_value_line(200, buf, WHITE);
}

static void update_encoder_page(const ChassisState_t *s)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "L_DELTA:%6d  TOTAL:%10ld", s->enc_left,
             (long)s->enc_left_total);
    ui_value_line(54, buf, WHITE);

    snprintf(buf, sizeof(buf), "R_DELTA:%6d  TOTAL:%10ld", s->enc_right,
             (long)s->enc_right_total);
    ui_value_line(72, buf, WHITE);

    snprintf(buf, sizeof(buf), "MAP L:PC6/7-T8 R:PA0/1-T2 P7:OFF");
    ui_value_line(100, buf, CYAN);

    snprintf(buf, sizeof(buf), "START(0=OK) L/R: %u / %u",
             (unsigned int)g_enc_l_status, (unsigned int)g_enc_r_status);
    ui_value_line(120, buf,
                  (g_enc_l_status == HAL_OK && g_enc_r_status == HAL_OK) ? GREEN : RED);

    snprintf(buf, sizeof(buf), "DES L/R : %4d / %4d",
             s->desired_left_count, s->desired_right_count);
    ui_value_line(148, buf, YELLOW);

    snprintf(buf, sizeof(buf), "TGT L/R : %4d / %4d",
             s->target_left_count, s->target_right_count);
    ui_value_line(166, buf, YELLOW);

    snprintf(buf, sizeof(buf), "PWM L/R:%5d/%5d  DRV:%s",
             s->pwm_left, s->pwm_right, s->driver_powered ? "ON" : "OFF");
    ui_value_line(190, buf, s->driver_powered ? CYAN : RED);
}

static void update_system_page(const ChassisState_t *s)
{
    char buf[64];
    uint32_t ms = HAL_GetTick();

    snprintf(buf, sizeof(buf), "UPTIME  : %lu.%03lu s", (unsigned long)(ms / 1000U), (unsigned long)(ms % 1000U));
    ui_value_line(170, buf, WHITE);

    snprintf(buf, sizeof(buf), "DRV PWR : %s  BAT=%.2fV", s->driver_powered ? "ON" : "OFF", s->battery_v);
    ui_value_line(190, buf, s->driver_powered ? GREEN : RED);

    snprintf(buf, sizeof(buf), "LAST CMD: %lu ms ago", (unsigned long)(ms - s->last_cmd_tick_ms));
    ui_value_line(210, buf, WHITE);
}

void DisplayUI_Init(void)
{
    LCD_Init();
    LCD_Display_Dir(1);       /* landscape, best for dashboard */
    LCD_Clear(BLACK);
    ui_clear_cache();

    g_page = UI_PAGE_MAIN;
    g_last_page = UI_PAGE_MAX;
    g_last_refresh_ms = 0U;
#if LCD_UI_AUTO_PAGE_MS > 0
    g_last_auto_page_ms = HAL_GetTick();
#endif
}

void DisplayUI_SetPage(UI_Page_t page)
{
    if (page >= UI_PAGE_MAX) return;
    g_page = page;
}

void DisplayUI_NextPage(void)
{
    g_page = (UI_Page_t)((g_page + 1U) % UI_PAGE_MAX);
}

UI_Page_t DisplayUI_GetPage(void)
{
    return g_page;
}

void DisplayUI_Task(void)
{
    uint32_t now = HAL_GetTick();
    const ChassisState_t *s;

#if LCD_UI_AUTO_PAGE_MS > 0
    if ((now - g_last_auto_page_ms) >= LCD_UI_AUTO_PAGE_MS)
    {
        g_last_auto_page_ms = now;
        DisplayUI_NextPage();
    }
#endif

    if (g_page != g_last_page)
    {
        draw_static_layout();
        g_last_page = g_page;
        g_last_refresh_ms = 0U;
    }

    if ((now - g_last_refresh_ms) < LCD_UI_REFRESH_MS)
    {
        return;
    }
    g_last_refresh_ms = now;

    s = Chassis_GetState();

    switch (g_page)
    {
        case UI_PAGE_MAIN:
            update_main_page(s);
            break;
        case UI_PAGE_ENCODER:
            update_encoder_page(s);
            break;
        case UI_PAGE_SYSTEM:
            update_system_page(s);
            break;
        case UI_PAGE_TUNE:
            update_tune_page();
            break;
        case UI_PAGE_TURN_TUNE:
            update_turn_tune_page();
            break;
        default:
            break;
    }
}
