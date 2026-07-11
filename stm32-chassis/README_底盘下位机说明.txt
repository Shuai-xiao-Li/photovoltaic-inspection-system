STM32F103ZET6 履带底盘 LCD 下位机工程说明
================================================

工程来源：正点原子“实验13 TFTLCD显示实验”工程体系。
当前版本：v8_encoder_diag，2.8寸LCD + KEY0/KEY1本地测试控制。

一、主要功能
------------------------------------------------
1. 保留正点原子2.8寸TFTLCD显示，不使用触摸。
2. LCD显示底盘状态：模式、电压、目标、编码器反馈、PWM、故障。
3. 使用KEY0/KEY1进行本地安全测试，无需串口即可控制。
4. 串口命令仍保留，可选用，不接串口也不影响按键测试。
5. LCD刷新改为300ms，且只在内容变化时重画，明显降低闪烁。

二、按键功能
------------------------------------------------
KEY0 = PE4，低电平有效
KEY1 = PE3，低电平有效
KEY_UP = PA0，不使用，因为PA0已经作为右编码器TIM2_CH1。

KEY0短按：选择下一个项目
KEY1短按：执行当前选中的项目
KEY0长按：切换LCD页面
KEY1长按：急停；如果已经有故障，则清除故障

LCD主页面下方会显示：
SELECT: 当前选中的项目
EVENT : 最近一次按键动作

可选择项目：
STOP
TEST LOW
TEST MID
TURN LEFT
TURN RIGHT
PAGE MAIN
PAGE ENC
PAGE SYS
ESTOP/CLEAR

三、推荐第一次测试步骤
------------------------------------------------
1. 只接STM32和LCD，确认LCD显示正常。
2. 架空履带底盘。
3. 按KEY0直到 SELECT 显示 TEST LOW。
4. 短按KEY1执行 TEST LOW。
5. 观察两侧履带是否低速、平稳转动。
6. 再按KEY0选择 STOP，短按KEY1停车。
7. 若异常，长按KEY1急停。

不要第一次就使用高速或长时间运行。
每次测试2到5秒后检查D153B和电机温度。

四、当前接线
------------------------------------------------
左履带 PWM：PA6 / TIM3_CH1 -> D153B PWMA
右履带 PWM：PA7 / TIM3_CH2 -> D153B PWMB
左方向：PE0 / PE1 -> AIN1 / AIN2
右方向：PE2 / PE6 -> BIN1 / BIN2
左编码器：PC6 / PC7 -> E1A / E1B，TIM8（避开精英板板载 24C02）
右编码器：PA0 / PA1 -> E2A / E2B，TIM2（必须拔掉精英板 P7 跳线帽）
电压采样：PA5 / ADC1_IN5 -> D153B ADC
LCD：正点原子TFTLCD接口，不使用触摸
KEY0：PE4
KEY1：PE3

五、LCD闪烁优化说明
------------------------------------------------
早期版本每100ms清除并重画多行动态信息，2.8寸TFTLCD上会有明显闪烁。
本版本做了两处优化：
1. board_config.h 中 LCD_UI_REFRESH_MS 改为 300U。
2. display_ui.c 中增加字符串缓存，只在显示内容变化时重画对应行，并用空格填充覆盖旧字符，避免整行LCD_Fill清屏。

如仍感觉闪烁，可以在 CHASSIS/board_config.h 中继续增大：
#define LCD_UI_REFRESH_MS 500U

六、注意
------------------------------------------------
本工程没有使用触摸；PB0/PB1交还给LCD/触摸相关资源，因此电机PWM改为PA6/PA7。
当前 MOTOR_PWM_LIMIT = 3800；初期测试仍应从 TEST LOW 开始，不要提高过快。


[v6_开环按键测试修正]
1. KEY0/KEY1 的 TEST LOW / TEST MID / TURN LEFT / TURN RIGHT 已改为开环 PWM 测试。
   原因：初次测试时编码器方向和 PID 未确认，闭环速度命令可能输出太小，电机不动。
2. PAGE MAIN / PAGE ENC / PAGE SYS 执行时会先 Chassis_Stop()，避免切页时电机命令残留。
3. 如果 D153B 电源关闭但 STM32 仍输出方向/PWM 高电平，D153B 电源灯微亮是 IO 反灌现象。
   正确测试流程：先 STOP，再关闭驱动电源；不要在驱动板断电时执行电机测试命令。

================ v7 安全修正版说明 ================
1. 解决 D153B 断电时电源灯微亮：
   - 程序通过 PA5 读取 D153B 的 ADC 电压，判断 D153B 是否上电。
   - 当 D153B 未上电时，PA6/PA7/PE0/PE1/PE2/PE6 会被配置为模拟输入/高阻状态。
   - 此时按 TEST LOW / TEST MID / TURN LEFT / TURN RIGHT 不会输出 PWM 或方向高电平。

2. v7 必须增加一根检测线：
   D153B ADC  -> STM32 PA5
   D153B GND  -> STM32 GND
   如果暂时不接 ADC，LCD 会显示 DRV:OFF，并拒绝电机测试。
   如必须临时绕过检测，可在 CHASSIS/board_config.h 中把 DRIVER_POWER_CHECK_ENABLE 改为 0U。

3. 低速不动的处理：
   - TEST LOW 从开环 PWM 900 提高到 1600。
   - TEST MID 提高到 2200。
   - TURN LEFT/RIGHT 使用 1600。
   - 架空起步实测后，非零最小有效 PWM / 速度环 FF 默认值设为 800。
   - 当前程序保留 PWM 限幅 3800 和斜坡加速，避免突然满速冲击。

4. 测试顺序：
   - 先只上 STM32：LCD 上应显示 DRV:OFF，按 TEST 不会让 D153B 灯亮。
   - 接 D153B 12V 和 ADC->PA5：LCD 应显示 DRV:ON。
   - 架空底盘，执行 TEST LOW，2 秒内 STOP。

================ v8 编码器诊断修正版说明 ================
1. 复核左编码器引脚映射：
   - D153B 随附 `接线说明.png` 及面向 STM32F103C8 最小板的官方例程使用：
     E1A / E1B -> PB6 / PB7 -> TIM4
     E2A / E2B -> PA0 / PA1 -> TIM2
   - 但 ALIENTEK 精英板的 IO 引脚分配表显示 PB6 / PB7 已连接板载
     24C02 的 SCL / SDA，带 4.7K 上拉，并标注不建议作为普通 IO 使用。
   - 本工程因此有意采用 PC6 / PC7 -> TIM8 作为左路编码器输入；这两个脚
     在不使用 OLED/CAMERA 接口时完全独立。
   - 接线必须是 E1A / E1B -> PC6 / PC7，而不是照搬厂商示例的 PB6 / PB7。
   - 右路 E2B 使用 PA1；精英板 PA1 还接 STM_ADC / TPAD，必须拔掉 P7
     跳线帽后再将 E2B 接入 PA1。

2. LCD 编码器页面新增 TOTAL 累计计数：
   - L_DELTA / R_DELTA 仍是速度环所需的最近 10ms 反馈值。
   - L_TOTAL / R_TOTAL 从开机累计，不会因为 LCD 每 300ms 才刷新一次而漏掉慢转脉冲。
   - 编码器页同时显示 DRV:ON/OFF；若为 OFF，电机测试命令不会输出 PWM。
   - 不启动电机也可手动向前转动履带检查；编码器必须已获得 3.3V~5V 供电且与 STM32 共地。
   - 正向转动时两个 TOTAL 都应持续增加；若某侧持续减少，再把对应 ENCODER_*_SIGN 改为 -1。

4. 当前方向校准结果：
   - 前进测试中左侧 F / L_DELTA 为负、右侧为正，因此已将
     ENCODER_LEFT_SIGN 设为 -1，ENCODER_RIGHT_SIGN 保持 1。

3. 本 LCD + 精英板工程与厂商原始接线表有三处有意不同：
   - 厂商示例 PWMA/PWMB 使用 PB1/PB0；本工程 LCD 占用 PB0，因此电机 PWM 必须接 PA6/PA7。
   - 厂商示例 ADC 使用 PA6；本工程 PA6 用作 PWM，因此 D153B ADC 必须接 PA5。
   - 厂商示例 E1A/E1B 使用 PB6/PB7；精英板该处接了 24C02，因此左编码器必须接 PC6/PC7。

