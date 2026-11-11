// 模拟量和数字量采集、时基设置、触发判断

#include "bsp_acquisition.h"
#include "adc.h"
#include "tim.h"
#include <string.h>

// 每个 DMA 半区存 500 组双通道数据
// 历史区保留 1000 点，找到触发点后从中取出 500 点
#define SAMPLES_PER_CH (ACQ_FRAME_POINTS * 2)
// 每组有 CH1、CH2 两个值，所以 1000 组需要 2000 个 uint16_t
#define ADC_DMA_BUF_LEN (SAMPLES_PER_CH * 2)

// ADC 接收缓冲区，CH1 和 CH2 交替存放
static uint16_t adc_buffer[ADC_DMA_BUF_LEN];

// 最近两次 DMA 回调的数据，共 1000 点
static uint16_t adc_history[ADC_DMA_BUF_LEN];
static uint8_t  digital_history[SAMPLES_PER_CH];

// 主循环处理这份副本，避免直接读取正在更新的历史区
static uint16_t adc_copy[ADC_DMA_BUF_LEN];
static uint8_t  digital_copy[SAMPLES_PER_CH];

// 奇数表示历史区正在更新，复制前后版本不同则重新等待
static volatile uint32_t history_version;
static uint8_t           history_blocks; // 收够两次回调后，历史区才有完整的 1000 点

// TIM2 中断中保存的数字电平
static volatile uint8_t  digital_buffer[SAMPLES_PER_CH];
static volatile uint16_t digital_index = 0; // 下一次数字采样写入的位置，范围 0～999

// 采集状态
static volatile uint8_t    frame_ready      = 0; // 只记录有新数据，不累计待发送帧数
static volatile uint32_t   overrun_count    = 0;
static volatile Timebase_t current_timebase = TB_1MS;
static volatile uint8_t    running          = 1;

// volatile 用在中断和主循环共用的状态上，读取时需要取当前值

// 默认触发：CH2、上升沿、AUTO，阈值 128
static TriggerConfig_t trigger = {.source    = TRIG_SRC_CH2,
                                  .edge      = TRIG_EDGE_RISING,
                                  .mode      = TRIG_MODE_AUTO,
                                  .level     = 128,
                                  .triggered = 0};

// 五档时基对应的 ARR 值
// TIM2 每 1 us 计数一次
// 采样间隔为 ARR + 1，单位 us
static const uint32_t sample_periods[TB_COUNT] = SCOPE_TIM2_ARR;

static const char *timebase_names[TB_COUNT] = {"10ms/div", "5ms/div", "1ms/div",
                                               "500us/div", "100us/div"};

static const char *source_names[] = {"CH1(Pot)", "CH2(Mic)", "DIG2(Square)",
                                     "DIG0(Sound)", "DIG1(Key)"};
// 这些名称用于状态回复，数组顺序要和对应枚举保持一致
static const char *edge_names[]   = {"RISING", "FALLING"};
static const char *mode_names[]   = {"AUTO", "NORMAL", "SINGLE"};

// ARR 预装载已关闭，改周期后将计数器清零
static void Acquisition_SetSamplePeriod(void)
{
    // 这里只改 ARR，预分频仍使用 CubeMX 设置的值
    __HAL_TIM_SET_AUTORELOAD(&htim2, sample_periods[current_timebase]);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    // 清掉原来的更新标志，不把旧标志当作新周期的第一次采样
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
}

// 改周期前先停止 TIM2 和 ADC DMA
// 这里不修改 running，运行或暂停由调用处设置
static void Acquisition_Restart(void)
{
    HAL_TIM_Base_Stop_IT(&htim2);
    // 先停止采样节拍，再停 DMA，清数据时就不会继续产生新采样
    HAL_ADC_Stop_DMA(&hadc3);

    Acquisition_SetSamplePeriod();

    // 模拟量和数字量都从头采，前后两种时基的数据不能混用
    digital_index     = 0;
    history_blocks    = 0;
    history_version   = 0;
    frame_ready       = 0;
    trigger.triggered = 0;

    // 先让 ADC 和 DMA 准备好，再启动 TIM2 产生采样触发
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buffer, ADC_DMA_BUF_LEN);
    HAL_TIM_Base_Start_IT(&htim2);
}

// 设置默认时基并启动采样
void Acquisition_Init(void)
{
    // 需要开启DAC的时钟，TIM2才能正常工作驱动ADC和读取数字通道，不知道为什么
    __HAL_RCC_DAC_CLK_ENABLE();

    current_timebase = TB_1MS;
    Acquisition_SetSamplePeriod();
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buffer, ADC_DMA_BUF_LEN);
    HAL_TIM_Base_Start_IT(&htim2);
}

// 换时基后重新采样，不沿用旧时基的数据
void Acquisition_SetTimebase(Timebase_t tb)
{
    current_timebase = tb;
    Acquisition_Restart();
}

// 暂停波形输出，底层采样仍继续
void Acquisition_Pause(void)
{
    running = 0;
}

void Acquisition_Resume(void)
{
    // 恢复时丢弃暂停期间的数据，等新数据采满后再输出
    Acquisition_Restart();
    running = 1;
}

uint8_t Acquisition_IsRunning(void)
{
    return running;
}

Timebase_t Acquisition_GetTimebase(void)
{
    return current_timebase;
}

const char *Acquisition_GetTimebaseName(Timebase_t tb)
{
    return timebase_names[tb];
}

// 检查是否有新数据，读取后清除标志
uint8_t Acquisition_IsFrameReady(void)
{
    if (frame_ready)
    {
        frame_ready = 0; // 这次更新只处理一次，后续更新由 DMA 回调再次置位
        return 1;
    }
    return 0;
}

uint32_t Acquisition_GetOvrErrorCount(void)
{
    return overrun_count;
}

// 触发比较统一用 8 位模拟量或 0、1 数字量
static uint8_t Read_TriggerValue(int index)
{
    // ADC 缓冲区中偶数位置是 CH1，奇数位置是 CH2
    // 右移 4 位，把 0～4095 换成 0～255，与串口下发的触发阈值对应
    switch (trigger.source)
    {
        case TRIG_SRC_CH1:
            return (uint8_t)(adc_copy[index * 2] >> 4);
        case TRIG_SRC_CH2:
            return (uint8_t)(adc_copy[index * 2 + 1] >> 4);
        case TRIG_SRC_DIG0:
            return digital_copy[index] & 1u;
        case TRIG_SRC_DIG2:
            return (digital_copy[index] >> 2) & 1u;
        default:
            return (digital_copy[index] >> 1) & 1u; // DIG1
    }
}

static int Check_Trigger_Edge(int prev_idx, int curr_idx)
{
    uint8_t prev = Read_TriggerValue(prev_idx);
    uint8_t curr = Read_TriggerValue(curr_idx);

    // 数字触发只看 0、1 跳变，不使用模拟触发阈值
    if (trigger.source >= TRIG_SRC_DIG2)
    {
        if (trigger.edge == TRIG_EDGE_RISING)
        {
            return prev == 0 && curr == 1;
        }
        return prev == 1 && curr == 0;
    }

    // 模拟触发要求相邻两点跨过阈值，停在阈值一侧不算触发
    if (trigger.edge == TRIG_EDGE_RISING)
    {
        return prev < trigger.level && curr >= trigger.level;
    }
    return prev > trigger.level && curr <= trigger.level;
}

// 从历史区前半段寻找触发边沿
static int Find_Trigger_Index(void)
{
    // 触发点后还要留够 500 点
    int search_limit = SAMPLES_PER_CH - ACQ_FRAME_POINTS;

    for (int i = 1; i <= search_limit; i++)
    {
        if (Check_Trigger_Edge(i - 1, i))
        {
            return i; // 从触发点开始输出波形
        }
    }

    return -1; // 没有找到触发边沿
}

// 复制前后版本相同，说明复制时历史区没有更新
static uint8_t Copy_Snapshot(void)
{
    uint32_t version = history_version;
    if (version & 1u)
    {
        // DMA 回调还没更新完，直接返回，不在这里等中断结束
        return 0;
    }
    // 让版本读取和数据复制按顺序完成，中断仍然可以正常执行
    __DMB();

    memcpy(adc_copy, adc_history, sizeof(adc_copy));
    memcpy(digital_copy, digital_history, sizeof(digital_copy));

    __DMB();

    // 复制时若被 DMA 回调更新了历史区，这份数据就不用
    return version == history_version;
}

// 复制数据、查找触发点，再取出 500 点并计算电压
uint8_t Acquisition_GetFrame(AcqFrame_t *frame)
{
    int start;
    uint32_t sum1 = 0, sum2 = 0;
    uint16_t minimum = 4095, maximum = 0;

    // 复制期间数据有变化，下一轮再取
    if (!Copy_Snapshot())
    {
        return 0;
    }
    // start 是输出起点，不是 DMA 当前写入的位置
    start = Find_Trigger_Index();
    trigger.triggered = start >= 0;

    if (start < 0)
    {
        // NORMAL 和 SINGLE 没等到边沿就返回，主循环仍会继续处理命令
        if (trigger.mode != TRIG_MODE_AUTO)
        {
            return 0;
        }
        start = 0; // AUTO 未触发时从索引 0 输出
    }

    frame->timebase = current_timebase; // 把时基和这帧数据一起交给发送函数

    for (int i = 0; i < ACQ_FRAME_POINTS; i++)
    {
        // 从触发点开始连续取 500 点，两路模拟量和数字量使用同一个索引
        int      index = start + i;
        uint16_t ch1   = adc_copy[index * 2];
        uint16_t ch2   = adc_copy[index * 2 + 1];

        // 波形传输用 8 位值，电压计算仍使用原来的 12 位值
        frame->points[i].ch1_raw8 = (uint8_t)(ch1 >> 4);
        frame->points[i].ch2_raw8 = (uint8_t)(ch2 >> 4);
        frame->points[i].dig_mask = digital_copy[index];

        sum1 += ch1;
        sum2 += ch2;

        if (ch2 < minimum)
        {
            minimum = ch2;
        }
        if (ch2 > maximum)
        {
            maximum = ch2;
        }
    }

    // 计算平均电压和 CH2 峰峰值
    // 12 位 ADC 满量程为 4095，这里按参考电压 3.3 V 换算
    frame->ch1_voltage = (sum1 / (float)ACQ_FRAME_POINTS) * (3.3f / 4095.0f);
    frame->ch2_voltage = (sum2 / (float)ACQ_FRAME_POINTS) * (3.3f / 4095.0f);
    frame->ch2_vpp     = (maximum - minimum) * (3.3f / 4095.0f);
    frame->digital     = frame->points[ACQ_FRAME_POINTS - 1].dig_mask; // 文本显示这一帧最后一点的电平

    if (trigger.mode == TRIG_MODE_SINGLE)
    {
        // 只停止后续取帧，这次已取得的波形仍由主循环发出去
        running = 0;
    }
    return 1;
}

// 这里只保存参数，重新采样由 Acquisition_Resume 处理
void Acquisition_SetTrigger(TrigSource_t src, TrigEdge_t edge, TrigMode_t mode,
                            uint8_t level)
{
    trigger.source = src;
    trigger.edge   = edge;
    trigger.mode   = mode;
    trigger.level  = level;
}

TriggerConfig_t *Acquisition_GetTriggerConfig(void)
{
    return &trigger;
}

// 重新等待一次触发
void Acquisition_ArmSingleTrigger(void)
{
    // ARM 不改触发源、边沿和阈值，沿用当前设置再采一次
    trigger.mode = TRIG_MODE_SINGLE;
    Acquisition_Resume();
}

const char *Acquisition_GetTrigSourceName(TrigSource_t src)
{
    return source_names[src];
}

const char *Acquisition_GetTrigEdgeName(TrigEdge_t edge)
{
    return edge_names[edge];
}

const char *Acquisition_GetTrigModeName(TrigMode_t mode)
{
    return mode_names[mode];
}

// TIM2 更新中断入口
// TRGO 触发 ADC 转换，中断中读取三路数字输入
uint8_t Acquisition_TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE); // 清除中断标志

        // 读取 GPIOI 和 GPIOF，PF8、PF9 共用一次端口读取
        // 最快档每 2 us 进一次中断，这里直接读端口以减少处理时间
        uint32_t port_i = GPIOI->IDR;
        uint32_t port_f = GPIOF->IDR;
		
        uint8_t mask = 0;
        // bit0：D0，PI0
        if (port_i & GPIO_PIN_0)
        {
            mask |= 0x01;
        }
        // bit1：D1，PF8
        if (port_f & GPIO_PIN_8)
        {
            mask |= 0x02;
        }
        // bit2：D2，PF9
        if (port_f & GPIO_PIN_9)
        {
            mask |= 0x04;
        }

        // 保存本次数字采样
        digital_buffer[digital_index] = mask;

        // 写满后回到缓冲区开头
        digital_index++;
        if (digital_index >= SAMPLES_PER_CH)
        {
            digital_index = 0;
        }
        return 1; // 本次更新中断已处理
    }
    return 0;
}

// 旧的后 500 点移到前面，再将新数据放入后半段
static void Publish_Block(uint16_t offset)
{
    // 更新前变成奇数，更新完再加一次，主循环据此判断复制是否被打断
    history_version++;
    __DMB();
    // 历史区前后各 500 点，移位时区域有重叠，所以使用 memmove
    memmove(adc_history, adc_history + ACQ_FRAME_POINTS * 2,
            ACQ_FRAME_POINTS * 2 * sizeof(uint16_t));
    memmove(digital_history, digital_history + ACQ_FRAME_POINTS, ACQ_FRAME_POINTS);
    // offset 按双通道采样组计数，一组有两个 ADC 值
    memcpy(adc_history + ACQ_FRAME_POINTS * 2, adc_buffer + offset * 2,
           ACQ_FRAME_POINTS * 2 * sizeof(uint16_t));
    for (uint16_t i = 0; i < ACQ_FRAME_POINTS; ++i)
    {
        // 数字缓冲区是 volatile，逐点复制对应半区的电平
        digital_history[ACQ_FRAME_POINTS + i] = digital_buffer[offset + i];
    }
    if (history_blocks < 2)
    {
        // 启动后的第一次回调只有 500 个新点，还不能用于触发取帧
        history_blocks++;
    }
    __DMB();
    history_version++;
    if (history_blocks == 2)
    {
        // 之后每更新半区都通知主循环，发送慢时使用后续更新的数据
        frame_ready = 1;
    }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        Publish_Block(0); // DMA 前半区已采满，读取前 500 组
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        Publish_Block(ACQ_FRAME_POINTS); // DMA 后半区已采满，读取后 500 组
    }
}

// ADC 出错后重新启动采样，清除旧数据和数字采样索引
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        overrun_count++; // 记录错误发生次数

        HAL_TIM_Base_Stop_IT(&htim2);
        HAL_ADC_Stop_DMA(hadc);
        digital_index  = 0;
        history_blocks = 0;
        frame_ready    = 0;
        __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_OVR); // 清除硬件 OVR 标志
        HAL_ADC_Start_DMA(hadc, (uint32_t *)adc_buffer, ADC_DMA_BUF_LEN); // 重启采样
        HAL_TIM_Base_Start_IT(&htim2);
    }
}
