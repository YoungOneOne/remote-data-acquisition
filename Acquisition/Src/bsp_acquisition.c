/**
  ******************************************************************************
  * @file    bsp_acquisition.c
  * @brief   ADC3 双通道模拟量采集、数字通道同步、时基变频与示波器触发实现
  ******************************************************************************
  */

#include "bsp_acquisition.h"
#include "adc.h"
#include "tim.h"
#include <string.h>

/* 数据流：ADC/DIG 循环缓冲区 -> 最近 1000 点历史 -> 主循环快照 -> 500 点输出。
 * 每个 DMA 半区有 500 个复合点，每点包含两路 ADC 值。
 * 保留 1000 点，是为了找到触发边沿后还能取出完整的 500 点波形。
 */
#define SAMPLES_PER_CH      (ACQ_FRAME_POINTS * 2)
#define ADC_DMA_BUF_LEN     (SAMPLES_PER_CH * 2)

// ADC3 DMA 接收缓冲区 (交替存放: [CH1_0, CH2_0, CH1_1, CH2_1, ...])
static uint16_t s_adc_dma_buffer[ADC_DMA_BUF_LEN];
/* DMA 回调更新历史；主循环只处理快照，防止边处理边被中断改写。
 * version 为奇数表示正在更新；复制前后版本相同才说明快照可用。
 */
static uint16_t s_history_adc[ADC_DMA_BUF_LEN];
static uint8_t s_history_dig[SAMPLES_PER_CH];
static uint16_t s_snapshot_adc[ADC_DMA_BUF_LEN];
static uint8_t s_snapshot_dig[SAMPLES_PER_CH];
static volatile uint32_t s_history_version;
static uint8_t s_history_blocks;
static uint8_t s_initialized;
static Timebase_t s_frame_tb;

// 数字通道同步缓冲区 (在 TIM2 中断中与 ADC 同步读取)
static volatile uint8_t  s_dig_buffer[SAMPLES_PER_CH];
static volatile uint16_t s_dig_sample_idx = 0;

// 系统运行状态标志
static volatile uint8_t    s_frame_ready_flag    = 0; // 一屏数据采满标志 (1:采满)
static volatile uint32_t   s_adc_ovr_error_count = 0; // ADC 溢出错误计数
static volatile Timebase_t s_current_tb          = TB_1MS; // 默认时基 1ms/div
static volatile uint8_t    s_is_running          = 1; // 运行/暂停标志

// 触发参数默认配置：麦克风 CH2、上升沿、自动模式 AUTO、阈值 128 (对应 1.65V 中间电平)
static TriggerConfig_t s_trig_cfg = {
    .source    = TRIG_SRC_CH2,
    .edge      = TRIG_EDGE_RISING,
    .mode      = TRIG_MODE_AUTO,
    .level     = 128,
    .triggered = 0
};

// 5 档时基对应的 TIM2 自动重载值 ARR 表
// TIM2 计数基准时钟是 1MHz (1us 计数一次)。
// 计数周期 us = ARR + 1。
static const uint32_t s_arr_table[TB_COUNT] = SCOPE_TIM2_ARR;

static const char *s_tb_names[TB_COUNT] = {
    "10ms/div", "5ms/div", "1ms/div", "500us/div", "100us/div"
};

static const char *s_src_names[]  = { "CH1(Pot)", "CH2(Mic)", "DIG2(Square)", "DIG0(Sound)", "DIG1(Key)" };
static const char *s_edge_names[] = { "RISING", "FALLING" };
static const char *s_mode_names[] = { "AUTO", "NORMAL", "SINGLE" };

// 初始化触发指示灯引脚 (PG7，扩展板插座【1-3】上的红色 LED)
static void Trigger_LED_Init(void)
{
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP; // 推挽输出
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOG, &gpio);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET); // 上电默认熄灭
}

// 采集底层外设与数据缓冲区初始化
void Acquisition_Init(void)
{
    // 1. 初始化 PG7 触发指示灯
    Trigger_LED_Init();

    // 2. 保留原工程的 DAC 时钟配置；本模块不使用 DAC 输出。
    __HAL_RCC_DAC_CLK_ENABLE();

    // 3. 初始时基设置为 1ms/div (配置 TIM2 的 ARR 为 19)
    Acquisition_SetTimebase(TB_1MS);

    // 4. 开启 ADC3 的 DMA 循环模式转换
    // 注意：此时 ADC 不会立即转换，而是在等待 TIM2 的 TRGO 触发脉冲
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)s_adc_dma_buffer, ADC_DMA_BUF_LEN);

    // 5. 开启 TIM2 的更新中断 (在中断里读取数字引脚，以定时器中断读取数字采样)
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);

    // 6. 启动定时器 TIM2 计数，开始持续产生硬件 TRGO 触发脉冲
    HAL_TIM_Base_Start(&htim2);
    s_initialized = 1;
}

// 动态设置扫描时基 (通过修改 TIM2 的 ARR 寄存器来变频)
void Acquisition_SetTimebase(Timebase_t tb)
{
    if (tb >= TB_COUNT) {
        tb = TB_10MS;
    }
    if (s_initialized) {
        HAL_TIM_Base_Stop(&htim2);
        HAL_ADC_Stop_DMA(&hadc3);
    }
    s_current_tb = tb;
    __HAL_TIM_SET_AUTORELOAD(&htim2, s_arr_table[tb]);
    htim2.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    s_dig_sample_idx = 0;
    s_history_blocks = 0;
    s_history_version = 0;
    s_frame_ready_flag = 0;
    s_trig_cfg.triggered = 0;
    if (s_initialized) {
        HAL_ADC_Start_DMA(&hadc3, (uint32_t *)s_adc_dma_buffer, ADC_DMA_BUF_LEN);
        HAL_TIM_Base_Start(&htim2);
    }
}

// 运行与暂停控制
void Acquisition_Pause(void)
{
    s_is_running = 0;
}

void Acquisition_Resume(void)
{
    s_trig_cfg.triggered = 0;
    Acquisition_SetTimebase(s_current_tb);
    s_is_running = 1;
}

uint8_t Acquisition_IsRunning(void)
{
    return s_is_running;
}

Timebase_t Acquisition_GetTimebase(void)
{
    return s_current_tb;
}

const char* Acquisition_GetTimebaseName(Timebase_t tb)
{
    if (tb >= TB_COUNT) return "Unknown";
    return s_tb_names[tb];
}

// 检查是否有一整屏数据采满 (读取后自动清除标志)
uint8_t Acquisition_IsFrameReady(void)
{
    if (s_frame_ready_flag) {
        s_frame_ready_flag = 0;
        return 1;
    }
    return 0;
}

uint32_t Acquisition_GetOvrErrorCount(void)
{
    return s_adc_ovr_error_count;
}

// ============================================================================
//                          示波器触发引擎核心算法
// ============================================================================

// 模拟通道取 8 位电压值，数字通道只取 0 或 1。
static uint8_t Read_TriggerValue(int index)
{
    switch (s_trig_cfg.source) {
    case TRIG_SRC_CH1:  return (uint8_t)(s_snapshot_adc[index * 2] >> 4);
    case TRIG_SRC_CH2:  return (uint8_t)(s_snapshot_adc[index * 2 + 1] >> 4);
    case TRIG_SRC_DIG0: return s_snapshot_dig[index] & 1u;
    case TRIG_SRC_DIG2: return (s_snapshot_dig[index] >> 2) & 1u;
    default:           return (s_snapshot_dig[index] >> 1) & 1u; // DIG1
    }
}

static int Check_Trigger_Edge(int prev_idx, int curr_idx)
{
    uint8_t prev = Read_TriggerValue(prev_idx);
    uint8_t curr = Read_TriggerValue(curr_idx);

    if (s_trig_cfg.source >= TRIG_SRC_DIG2) {
        if (s_trig_cfg.edge == TRIG_EDGE_RISING) return prev == 0 && curr == 1;
        return prev == 1 && curr == 0;
    }

    if (s_trig_cfg.edge == TRIG_EDGE_RISING)
        return prev < s_trig_cfg.level && curr >= s_trig_cfg.level;
    return prev > s_trig_cfg.level && curr <= s_trig_cfg.level;
}

// 在缓冲区前段寻找第一个满足触发条件的采样点索引
static int Find_Trigger_Index(void)
{
    // 只搜索后面还够取 500 点的位置，避免越界或拼接旧数据。
    int search_limit = SAMPLES_PER_CH - ACQ_FRAME_POINTS;

    for (int i = 1; i <= search_limit; i++) {
        if (Check_Trigger_Edge(i - 1, i)) {
            return i; // 找到触发点，返回它的索引作为波形起始点
        }
    }

    return -1; // 整个窗口内未搜索到边沿
}

// 中断可能在复制期间更新历史；版本号没变，才使用这份快照。
static uint8_t Copy_Snapshot(void)
{
    uint32_t version = s_history_version;
    if (version & 1u) return 0;
    __DMB();
    memcpy(s_snapshot_adc, s_history_adc, sizeof(s_snapshot_adc));
    memcpy(s_snapshot_dig, s_history_dig, sizeof(s_snapshot_dig));
    __DMB();
    return version == s_history_version;
}

// 找触发点 -> 取连续 500 点 -> 求平均电压和峰峰值。
uint8_t Acquisition_GetFrame(AcqPoint_t *out_points, float *out_ch1_v, float *out_ch2_v, float *out_ch2_vpp, uint8_t *out_dig_mask)
{
    if (!Copy_Snapshot()) return 0;
    s_frame_tb = s_current_tb;
    // 步骤 1：在数据缓冲区中搜索触发点
    int trig_idx = Find_Trigger_Index();

    if (trig_idx >= 0) {
        // 捕获成功：点亮 PG7 红色触发指示灯
        s_trig_cfg.triggered = 1;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_SET);
    } else {
        // 未捕获到边沿：熄灭 PG7 触发指示灯
        s_trig_cfg.triggered = 0;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);

        // 如果是 NORMAL (常规) 或 SINGLE (单次) 模式：
        // 没有检测到信号时，必须保持屏幕波形定格静止，不输出新帧
        if (s_trig_cfg.mode == TRIG_MODE_NORMAL || s_trig_cfg.mode == TRIG_MODE_SINGLE) {
            return 0;
        }

        // 如果是 AUTO (自动) 模式：
        // 即使没有触发边沿，也从第 0 个点开始强制刷新，屏幕不卡死
        trig_idx = 0;
    }

    // 步骤 2：从触发对齐点连续提取 500 个采样点，并统计累加和
    uint32_t sum1 = 0;
    uint32_t sum2 = 0;
    uint16_t ch2_max = 0;
    uint16_t ch2_min = 4095;

    for (int i = 0; i < ACQ_FRAME_POINTS; i++) {
        int src = trig_idx + i;
        uint16_t raw_ch1 = s_snapshot_adc[src * 2];     // CH1 电位器原始 12 位值
        uint16_t raw_ch2 = s_snapshot_adc[src * 2 + 1]; // CH2 麦克风原始 12 位值

        // 累加求均值
        sum1 += raw_ch1;
        sum2 += raw_ch2;

        // 寻找麦克风通道的最大值和最小值 (用于计算 Vpp 峰峰值)
        if (raw_ch2 > ch2_max) ch2_max = raw_ch2;
        if (raw_ch2 < ch2_min) ch2_min = raw_ch2;

        // 打包当前点数据送出
        if (out_points) {
            out_points[i].ch1_raw8 = (uint8_t)(raw_ch1 >> 4); // 压缩为 8 位 (0~255)
            out_points[i].ch2_raw8 = (uint8_t)(raw_ch2 >> 4);
            out_points[i].dig_mask = s_snapshot_dig[src];       // 该点对应的 3 路数字引脚状态
        }
    }

    // 步骤 3：计算并返回物理量参数
    if (out_dig_mask) {
        *out_dig_mask = s_snapshot_dig[trig_idx + ACQ_FRAME_POINTS - 1];
    }
    // 电压转换公式：平均值 = (采样累加和 / 点数) * (参考电压 3.3V / 4095)
    if (out_ch1_v) {
        *out_ch1_v = (sum1 / (float)ACQ_FRAME_POINTS) * (3.30f / 4095.0f);
    }
    if (out_ch2_v) {
        *out_ch2_v = (sum2 / (float)ACQ_FRAME_POINTS) * (3.30f / 4095.0f);
    }
    // 峰峰值公式：Vpp = (最大值 - 最小值) * (3.3V / 4095)
    if (out_ch2_vpp) {
        *out_ch2_vpp = (ch2_max - ch2_min) * (3.30f / 4095.0f);
    }

    // 步骤 4：单次模式 (SINGLE) 处理
    // 成功捕获一帧后停止输出新帧；底层 DMA 仍继续采样。
    if (s_trig_cfg.mode == TRIG_MODE_SINGLE) {
        s_is_running = 0;
    }

    return 1; // 成功输出一帧有效波形
}

// 触发参数设置
void Acquisition_SetTrigger(TrigSource_t src, TrigEdge_t edge, TrigMode_t mode, uint8_t level)
{
    if (src < TRIG_SRC_COUNT) {
        s_trig_cfg.source = src;
    }
    if (edge <= TRIG_EDGE_FALLING) {
        s_trig_cfg.edge = edge;
    }
    if (mode <= TRIG_MODE_SINGLE) {
        s_trig_cfg.mode = mode;
    }
    s_trig_cfg.level = level;

    s_trig_cfg.triggered = 0;
    Acquisition_SetTimebase(s_current_tb);
    if (mode == TRIG_MODE_SINGLE) s_is_running = 1;
}

TriggerConfig_t* Acquisition_GetTriggerConfig(void)
{
    return &s_trig_cfg;
}

// 单次触发重新装弹 (ARM)
void Acquisition_ArmSingleTrigger(void)
{
    Acquisition_SetTimebase(s_current_tb);
    s_trig_cfg.mode      = TRIG_MODE_SINGLE;
    s_trig_cfg.triggered = 0; // 清除已触发标志
    s_is_running         = 1; // 允许采集，等待下一次触发到来
}

const char* Acquisition_GetTrigSourceName(TrigSource_t src)
{
    if (src < TRIG_SRC_COUNT) return s_src_names[src];
    return "UNKNOWN";
}

const char* Acquisition_GetTrigEdgeName(TrigEdge_t edge)
{
    if (edge <= TRIG_EDGE_FALLING) return s_edge_names[edge];
    return "UNKNOWN";
}

const char* Acquisition_GetTrigModeName(TrigMode_t mode)
{
    if (mode <= TRIG_MODE_SINGLE) return s_mode_names[mode];
    return "UNKNOWN";
}

// ============================================================================
//                          硬件中断与回调处理
// ============================================================================

// 定时器 TIM2 更新中断服务函数
// TIM2 产生硬件 TRGO 触发 ADC 采样的同时，该中断同时执行，读取 3 路数字通道电平
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR = ~TIM_SR_UIF; // 清除中断标志

        // 直接读取端口输入数据寄存器 (IDR)
        // 这样可以一次性读到引脚电平，数字采样相对 ADC 的偏差需实机测量
        uint32_t pi = GPIOI->IDR;
        uint32_t pf = GPIOF->IDR;

        uint8_t mask = 0;
        // Bit 0: DIG0 声音门限 DO (PI0 引脚)
        if (pi & GPIO_PIN_0) {
            mask |= 0x01;
        }
        // Bit 1: DIG1 独立按键 (PF8 引脚，开启内部上拉，按下为低电平 0)
        if (pf & GPIO_PIN_8) {
            mask |= 0x02;
        }
        // Bit 2: DIG2 方波回环测试 (PI2 引脚)
        if (pi & GPIO_PIN_2) {
            mask |= 0x04;
        }

        // 存入数字采样缓冲区
        s_dig_buffer[s_dig_sample_idx] = mask;

        // 指针循环递增
        s_dig_sample_idx++;
        if (s_dig_sample_idx >= SAMPLES_PER_CH) {
            s_dig_sample_idx = 0;
        }
    }
}

// DMA 每完成半区，就把这 500 点放到历史后半段，旧后半段移到前面。
static void Publish_Block(uint16_t offset)
{
    s_history_version++;
    __DMB();
    memmove(s_history_adc, s_history_adc + ACQ_FRAME_POINTS * 2,
            ACQ_FRAME_POINTS * 2 * sizeof(uint16_t));
    memmove(s_history_dig, s_history_dig + ACQ_FRAME_POINTS, ACQ_FRAME_POINTS);
    memcpy(s_history_adc + ACQ_FRAME_POINTS * 2, s_adc_dma_buffer + offset * 2,
           ACQ_FRAME_POINTS * 2 * sizeof(uint16_t));
    for (uint16_t i = 0; i < ACQ_FRAME_POINTS; ++i)
        s_history_dig[ACQ_FRAME_POINTS + i] = s_dig_buffer[offset + i];
    if (s_history_blocks < 2) s_history_blocks++;
    __DMB();
    s_history_version++;
    if (s_history_blocks == 2) s_frame_ready_flag = 1;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3) Publish_Block(0);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3) Publish_Block(ACQ_FRAME_POINTS);
}

Timebase_t Acquisition_GetFrameTimebase(void)
{
    return s_frame_tb;
}

// ADC 硬件溢出中断回调函数 (自愈保护)
// 在极高采样率下如果偶尔发生总线拥堵，ADC 会报 OVR (Overrun) 溢出并封锁转换
// 此处清除标志并重新启动 DMA，防止单片机死锁
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3) {
        s_adc_ovr_error_count++; // 记录错误发生次数

        HAL_TIM_Base_Stop(&htim2);
        HAL_ADC_Stop_DMA(hadc);
        s_dig_sample_idx = 0;
        s_history_blocks = 0;
        s_frame_ready_flag = 0;
        __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_OVR); // 清除硬件 OVR 标志
        HAL_ADC_Start_DMA(hadc, (uint32_t *)s_adc_dma_buffer, ADC_DMA_BUF_LEN); // 重启采样
        HAL_TIM_Base_Start(&htim2);
    }
}
