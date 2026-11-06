/**
  ******************************************************************************
  * @file    bsp_protocol.c
  * @brief   串口双向通信协议驱动 (支持 1513 字节二进制示波器帧与文本调试输出)
  ******************************************************************************
  */

#include "bsp_protocol.h"
#include "bsp_generator.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 默认上电工作在二进制示波器帧模式 (MODE 1，与显示板 B 及 Python 模拟器配套)
static DMA_HandleTypeDef s_tx_dma;
static DMA_HandleTypeDef s_vcp_tx_dma;
static uint32_t s_frame_sequence;
/* 两路 DMA 共用发送缓冲区，必须等两路都发送完才能重用。 */
uint8_t Protocol_IsBusy(void)
{
    return huart6.gState != HAL_UART_STATE_READY ||
           huart1.gState != HAL_UART_STATE_READY;
}

static ProtocolMode_t s_current_mode = PROTOCOL_MODE_BINARY;

// ============================================================================
//                          串口接收中断与环形队列
// ============================================================================
// 为什么需要环形队列 (Ring Buffer)？
// 波形由 DMA 发送，发送期间暂缓执行命令，避免回复文本插入波形包。
// 接收中断先把命令字节存入队列，主循环等发送结束后再处理。队列满时会丢弃新字节。
#define UART_RX_RING_SIZE   256
static volatile uint8_t  s_rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0; // 写指针 (中断中推进)
static volatile uint16_t s_rx_tail = 0; // 读指针 (主循环中推进)

// 重定向 C 语言标准库 printf 函数到串口输出
// 同时输出到 USART1 (ST-Link 电脑虚拟串口) 和 USART6 (双板直联串口)
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    if (!Protocol_IsBusy()) HAL_UART_Transmit(&huart6, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* 两个串口的接收流程相同，收到的字节先存入队列。 */
static void Protocol_ReceiveUart(USART_TypeDef *uart)
{
    if (uart->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
        uart->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;
    while (uart->ISR & USART_ISR_RXNE) {
        uint8_t byte = (uint8_t)(uart->RDR & 0xFF);
        uint16_t next = (s_rx_head + 1) % UART_RX_RING_SIZE;
        if (next != s_rx_tail) {
            s_rx_ring[s_rx_head] = byte;
            s_rx_head = next;
        }
    }
}

void Protocol_UartRx_ISR(void)
{
    Protocol_ReceiveUart(USART1);
}

void Protocol_Uart6Rx_ISR(void)
{
    Protocol_ReceiveUart(USART6);
}

// 从环形队列中读取一个字符 (返回 1 表示读到数据，0 表示队列为空)
static int Protocol_RingRead(uint8_t *byte)
{
    if (s_rx_head == s_rx_tail) {
        return 0; // 队列为空
    }
    *byte = s_rx_ring[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % UART_RX_RING_SIZE;
    return 1;
}

// 协议模块初始化
void Protocol_Init(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
    __HAL_RCC_DMA2_CLK_ENABLE();
    s_tx_dma.Instance = DMA2_Stream6;
    s_tx_dma.Init.Channel = DMA_CHANNEL_5;
    s_tx_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    s_tx_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    s_tx_dma.Init.MemInc = DMA_MINC_ENABLE;
    s_tx_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    s_tx_dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    s_tx_dma.Init.Mode = DMA_NORMAL;
    s_tx_dma.Init.Priority = DMA_PRIORITY_LOW;
    s_tx_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&s_tx_dma) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(&huart6, hdmatx, s_tx_dma);
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    s_vcp_tx_dma.Init = s_tx_dma.Init;
    s_vcp_tx_dma.Instance = DMA2_Stream7;
    s_vcp_tx_dma.Init.Channel = DMA_CHANNEL_4;
    s_vcp_tx_dma.State = HAL_DMA_STATE_RESET;
    s_vcp_tx_dma.Lock = HAL_UNLOCKED;
    if (HAL_DMA_Init(&s_vcp_tx_dma) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(&huart1, hdmatx, s_vcp_tx_dma);
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    // 开启 USART1 (电脑串口) 的接收中断与错误中断
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_ERR);

    // 开启 USART6 (显示板直联串口) 的接收中断与错误中断
    HAL_NVIC_SetPriority(USART6_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_ERR);
}

void Protocol_SetMode(ProtocolMode_t mode)
{
    s_current_mode = mode;
}

ProtocolMode_t Protocol_GetMode(void)
{
    return s_current_mode;
}

// 辅助字符串处理：去除前后的空格，并将所有字母转换为大写
static void str_trim_and_upper(char *dst, const char *src, size_t max_len)
{
    // 跳过开头的空格
    while (*src == ' ' || *src == '\t') {
        src++;
    }

    size_t i = 0;
    while (*src && i < max_len - 1) {
        char c = *src++;
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A'; // 转大写
        }
        dst[i++] = c;
    }

    // 去除末尾的空格
    while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t')) {
        i--;
    }
    dst[i] = '\0';
}

// ============================================================================
//                          反向控制指令解析与执行
// ============================================================================
// 跳过命令名后面的分隔符，兼容 TB 2、TB=2、TB:2 这几种写法。
static const char *Protocol_CommandValue(const char *text)
{
    while (*text == ' ' || *text == '=' || *text == ':') text++;
    return text;
}

// 一次提交完整触发参数
static void Protocol_CommandTrigger(const char *cmd)
{
    unsigned int src, edge, mode, level;
    char extra;
    if (sscanf(cmd + 5, "%u %u %u %u %c", &src, &edge, &mode, &level, &extra) != 4 ||
        src >= TRIG_SRC_COUNT || edge > 1 || mode > 2 || level > 255) {
        printf("$ERR:TRIG_INVALID\r\n");
        return;
    }
    Acquisition_SetTrigger((TrigSource_t)src, (TrigEdge_t)edge, (TrigMode_t)mode, (uint8_t)level);
    Acquisition_Resume();
    printf("$OK:TRIG=%u,%u,%u,%u\r\n", src, edge, mode, level);
    return;
}

// 设置时基
static void Protocol_CommandTimebase(const char *cmd)
{
    const char *p = Protocol_CommandValue(cmd + 2);
    int tb = atoi(p);
    if (tb >= 0 && tb < TB_COUNT) {
        Acquisition_SetTimebase((Timebase_t)tb);
        printf("$OK:TB=%d\r\n", tb);
    } else {
        printf("$ERR:TB_RANGE\r\n");
    }
}

// 控制方波输出
static void Protocol_CommandGenerator(const char *cmd)
{
    if (strcmp(cmd, "GEN_START") == 0 || strcmp(cmd, "GEN START") == 0 || strcmp(cmd, "GEN ON") == 0) {
        Generator_Start();
        printf("$OK:GEN_START\r\n");
    } else if (strcmp(cmd, "GEN_STOP") == 0 || strcmp(cmd, "GEN STOP") == 0 || strcmp(cmd, "GEN OFF") == 0) {
        Generator_Stop();
        printf("$OK:GEN_STOP\r\n");
    } else if (strcmp(cmd, "GEN_NEXT") == 0) {
        uint32_t f = Generator_NextPreset();
        printf("$OK:GEN=%u\r\n", (unsigned int)f);
    } else if (strcmp(cmd, "GEN_PREV") == 0) {
        uint32_t f = Generator_PrevPreset();
        printf("$OK:GEN=%u\r\n", (unsigned int)f);
    } else {
        // 解析具体的频率数值
        const char *p = Protocol_CommandValue(cmd + 3);
        int freq = atoi(p);
        if (freq >= GEN_FREQ_MIN && freq <= GEN_FREQ_MAX) {
            Generator_SetFrequency((uint32_t)freq);
            printf("$OK:GEN=%d\r\n", freq);
        } else {
            printf("$ERR:GEN_RANGE\r\n");
        }
    }
}

// 切换文本或二进制输出
static void Protocol_CommandMode(const char *cmd)
{
    const char *p = Protocol_CommandValue(cmd + 4);
    if (*p == '1' || strncmp(p, "BINARY", 6) == 0 || strncmp(p, "BIN", 3) == 0) {
        s_current_mode = PROTOCOL_MODE_BINARY;
        printf("$OK:MODE_BINARY\r\n");
    } else if (*p == '0' || strncmp(p, "TEXT", 4) == 0 || strncmp(p, "TXT", 3) == 0) {
        s_current_mode = PROTOCOL_MODE_TEXT;
        printf("$OK:MODE_TEXT\r\n");
    } else {
        printf("$ERR:MODE_INVALID\r\n");
    }
}

// 设置触发模式（兼容旧指令）
static void Protocol_CommandTriggerMode(const char *cmd)
{
    const char *p = Protocol_CommandValue(cmd + 9);
    TriggerConfig_t *cfg = Acquisition_GetTriggerConfig();
    TrigMode_t mode;
    if (strcmp(p, "AUTO") == 0 || strcmp(p, "0") == 0) {
        mode = TRIG_MODE_AUTO;
    } else if (strcmp(p, "NORMAL") == 0 || strcmp(p, "NORM") == 0 || strcmp(p, "1") == 0) {
        mode = TRIG_MODE_NORMAL;
    } else if (strcmp(p, "SINGLE") == 0 || strcmp(p, "2") == 0) {
        mode = TRIG_MODE_SINGLE;
    } else {
        printf("$ERR:TRIG_MODE_INVALID\r\n");
        return;
    }
    Acquisition_SetTrigger(cfg->source, cfg->edge, mode, cfg->level);
    printf("$OK:TRIG_MODE=%s\r\n", Acquisition_GetTrigModeName(mode));
}

// 设置触发源（兼容旧指令）
static void Protocol_CommandTriggerSource(const char *cmd)
{
    const char *p = Protocol_CommandValue(cmd + 8);
    TriggerConfig_t *cfg = Acquisition_GetTriggerConfig();
    TrigSource_t source;
    const char *name;
    if (strcmp(p, "CH1") == 0 || strcmp(p, "0") == 0) {
        source = TRIG_SRC_CH1; name = "CH1";
    } else if (strcmp(p, "CH2") == 0 || strcmp(p, "1") == 0) {
        source = TRIG_SRC_CH2; name = "CH2";
    } else if (strcmp(p, "DIG2") == 0 || strcmp(p, "2") == 0) {
        source = TRIG_SRC_DIG2; name = "DIG2";
    } else if (strcmp(p, "DIG0") == 0 || strcmp(p, "3") == 0) {
        source = TRIG_SRC_DIG0; name = "DIG0";
    } else if (strcmp(p, "DIG1") == 0 || strcmp(p, "4") == 0) {
        source = TRIG_SRC_DIG1; name = "DIG1";
    } else {
        printf("$ERR:TRIG_SRC_INVALID\r\n");
        return;
    }
    Acquisition_SetTrigger(source, cfg->edge, cfg->mode, cfg->level);
    printf("$OK:TRIG_SRC=%s\r\n", name);
}

// 设置触发边沿（兼容旧指令）
static void Protocol_CommandTriggerEdge(const char *cmd)
{
    const char *p = Protocol_CommandValue(cmd + 9);
    TriggerConfig_t *cfg = Acquisition_GetTriggerConfig();
    if (strncmp(p, "RISE", 4) == 0 || strcmp(p, "RISING") == 0 || strcmp(p, "0") == 0) {
        Acquisition_SetTrigger(cfg->source, TRIG_EDGE_RISING, cfg->mode, cfg->level);
        printf("$OK:TRIG_EDGE=RISING\r\n");
    } else if (strncmp(p, "FALL", 4) == 0 || strcmp(p, "1") == 0) {
        Acquisition_SetTrigger(cfg->source, TRIG_EDGE_FALLING, cfg->mode, cfg->level);
        printf("$OK:TRIG_EDGE=FALLING\r\n");
    } else {
        printf("$ERR:TRIG_EDGE_INVALID\r\n");
    }
}

// 设置触发电平（兼容旧指令）
static void Protocol_CommandTriggerLevel(const char *cmd)
{
    const char *p = (strncmp(cmd, "TRIG_LEVEL", 10) == 0) ? (cmd + 10) : (cmd + 8);
    p = Protocol_CommandValue(p);
    int lvl = atoi(p);
    if (lvl >= 0 && lvl <= 255) {
        TriggerConfig_t *cfg = Acquisition_GetTriggerConfig();
        Acquisition_SetTrigger(cfg->source, cfg->edge, cfg->mode, (uint8_t)lvl);
        printf("$OK:TRIG_LVL=%d\r\n", lvl);
    } else {
        printf("$ERR:TRIG_LVL_RANGE\r\n");
    }
}

static void Protocol_ExecuteCommand(const char *cmd_line)
{
    char cmd[64];
    str_trim_and_upper(cmd, cmd_line, sizeof(cmd));

    if (cmd[0] == '\0') {
        return; // 空指令直接忽略
    }

    /* 新版显示端使用这条指令，避免分别修改参数时产生过渡帧。 */
    if (strncmp(cmd, "TRIG ", 5) == 0) {
        Protocol_CommandTrigger(cmd);
        return;
    }

    // 1. 时基设置指令: 例如 "TB 2" 或 "TB=2"
    if (strncmp(cmd, "TB", 2) == 0) {
        Protocol_CommandTimebase(cmd);
    }
    // 2. 方波发生器控制指令: 例如 "GEN 2000" 或 "GEN_START" / "GEN_STOP"
    else if (strncmp(cmd, "GEN", 3) == 0) {
        Protocol_CommandGenerator(cmd);
    }
    // 3. 运行 / 暂停指令
    else if (strcmp(cmd, "RUN") == 0 || strcmp(cmd, "START") == 0) {
        Acquisition_Resume();
        printf("$OK:RUN\r\n");
    } else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "PAUSE") == 0) {
        Acquisition_Pause();
        printf("$OK:STOP\r\n");
    }
    // 4. 传输模式切换: MODE 0 (文本调试) / MODE 1 (二进制示波器帧)
    else if (strncmp(cmd, "MODE", 4) == 0) {
        Protocol_CommandMode(cmd);
    }
    // 5. 触发模式设置: TRIG_MODE AUTO / NORMAL / SINGLE
    else if (strncmp(cmd, "TRIG_MODE", 9) == 0) {
        Protocol_CommandTriggerMode(cmd);
    }
    // 6. 触发源设置: CH1 / CH2 / DIG0 / DIG1 / DIG2
    else if (strncmp(cmd, "TRIG_SRC", 8) == 0) {
        Protocol_CommandTriggerSource(cmd);
    }
    // 7. 触发边沿设置: TRIG_EDGE RISE / FALL
    else if (strncmp(cmd, "TRIG_EDGE", 9) == 0) {
        Protocol_CommandTriggerEdge(cmd);
    }
    // 8. 触发阈值设置: TRIG_LVL 0~255
    else if (strncmp(cmd, "TRIG_LVL", 8) == 0 || strncmp(cmd, "TRIG_LEVEL", 10) == 0) {
        Protocol_CommandTriggerLevel(cmd);
    }
    // 9. 单次触发重新准备: ARM 或 SINGLE
    else if (strcmp(cmd, "ARM") == 0 || strcmp(cmd, "SINGLE") == 0) {
        Acquisition_ArmSingleTrigger();
        printf("$OK:ARM_SINGLE\r\n");
    }
    // 10. 全局状态同步查询: SYNC 或 STATUS
    else if (strcmp(cmd, "SYNC") == 0 || strcmp(cmd, "STATUS") == 0) {
        TriggerConfig_t *trig = Acquisition_GetTriggerConfig();
        printf("$STATE,TB:%d,MODE:%d,GEN:%u,RUN:%d,TRIG:%s,%s,%s,LVL:%u,TRGD:%d\r\n",
               Acquisition_GetTimebase(),
               s_current_mode,
               (unsigned int)Generator_GetFrequency(),
               Acquisition_IsRunning(),
               Acquisition_GetTrigSourceName(trig->source),
               Acquisition_GetTrigEdgeName(trig->edge),
               Acquisition_GetTrigModeName(trig->mode),
               trig->level,
               trig->triggered);
    }
    else {
        printf("$ERR:UNKNOWN_CMD:%s\r\n", cmd);
    }
}

// 轮询检查环形队列中的字符并组包解析
void Protocol_ProcessRx(void)
{
    if (Protocol_IsBusy()) return;
    static char rx_buf[64];
    static uint8_t rx_idx = 0;
    static uint32_t last_rx_tick = 0;
    uint8_t ch;

    // 超时清空防死锁机制：
    // 如果缓冲区已经有字符，但超过 100ms 还没收到换行符，说明可能出现丢包，清空等待下一包
    if (rx_idx > 0 && (HAL_GetTick() - last_rx_tick > 100)) {
        rx_idx = 0;
    }

    // 从接收环形队列中逐字节读出
    while (Protocol_RingRead(&ch)) {
        last_rx_tick = HAL_GetTick();

        // 遇到回车符或换行符，说明一行完整的指令接收完成
        if (ch == '\r' || ch == '\n') {
            if (rx_idx > 0) {
                rx_buf[rx_idx] = '\0'; // 加上字符串结束符
                Protocol_ExecuteCommand(rx_buf); // 解析并执行指令
                rx_idx = 0; // 重置行缓冲区
            }
        } else {
            // 忽略指令开头无意义的空格或 Tab
            if (rx_idx == 0 && (ch == ' ' || ch == '\t')) {
                continue;
            }
            // 存入行缓冲区 (防止溢出)
            if (rx_idx < sizeof(rx_buf) - 1) {
                rx_buf[rx_idx++] = (char)ch;
            }
        }
    }
}

// ============================================================================
//                          下行波形数据发送协议
// ============================================================================

// Wave v2: AA55, type=02, tb, count LE16, seq LE32, payload, XOR, CRLF.
void Protocol_SendBinaryFrame(Timebase_t tb, const AcqPoint_t *points, uint16_t count)
{
    static uint8_t tx_buf[SCOPE_FRAME_BYTES];
    uint16_t idx = 0;

    if (count != ACQ_FRAME_POINTS || Protocol_IsBusy()) return;
    // 1. 组装帧头与信息
    tx_buf[idx++] = SCOPE_HEAD0;
    tx_buf[idx++] = SCOPE_HEAD1;
    tx_buf[idx++] = SCOPE_WAVE_TYPE;            // 帧类型：波形数据
    tx_buf[idx++] = (uint8_t)tb;     // 当前扫描时基索引
    tx_buf[idx++] = (uint8_t)count;
    tx_buf[idx++] = (uint8_t)(count >> 8);
    for (uint8_t b = 0; b < 4; ++b)
        tx_buf[idx++] = (uint8_t)(s_frame_sequence >> (8 * b));
    s_frame_sequence++;

    // 2. 填充 500 个采样点 (每个点 3 字节)
    for (uint16_t i = 0; i < count; i++) {
        tx_buf[idx++] = points[i].ch1_raw8; // CH1 电位器
        tx_buf[idx++] = points[i].ch2_raw8; // CH2 麦克风
        tx_buf[idx++] = points[i].dig_mask; // 3 路数字引脚
    }

    // 3. 计算异或校验和 (从偏移 2 的 Type 一直异或到数据末尾)
    uint8_t xor_sum = 0;
    for (uint16_t i = 2; i < idx; i++) {
        xor_sum ^= tx_buf[i];
    }
    tx_buf[idx++] = xor_sum;

    // 4. 组装帧尾
    tx_buf[idx++] = 0x0D;            // '\r'
    tx_buf[idx++] = 0x0A;            // '\n'

    // 两路 DMA 同时读取此缓冲区；两路发送完成前不能修改。
    HAL_UART_Transmit_DMA(&huart6, tx_buf, idx);
    HAL_UART_Transmit_DMA(&huart1, tx_buf, idx);
}

// 文本调试流输出 (MODE 0，适合普通串口助手直接查看)
void Protocol_PrintDebugText(Timebase_t tb, float ch1_v, float ch2_v, float ch2_vpp, uint8_t dig_mask, uint32_t ovr_err)
{
    TriggerConfig_t *trig = Acquisition_GetTriggerConfig();
    uint8_t d0 = (dig_mask & 0x01) ? 1 : 0;
    uint8_t d1 = (dig_mask & 0x02) ? 1 : 0;
    uint8_t d2 = (dig_mask & 0x04) ? 1 : 0;

    printf("[%s] Trig:[%s %s %s L:%u %s] | CH1:%1.3fV | CH2:%1.3fV(Vpp:%1.3fV) | DIG:[D0:%d D1:%d D2:%d] | OVR:%u\r\n",
           Acquisition_GetTimebaseName(tb),
           Acquisition_GetTrigSourceName(trig->source),
           Acquisition_GetTrigEdgeName(trig->edge),
           Acquisition_GetTrigModeName(trig->mode),
           trig->level,
           trig->triggered ? "OK" : "WAIT",
           ch1_v, ch2_v, ch2_vpp, d0, d1, d2, ovr_err);
}

// 统一调度输出函数 (根据当前工作模式自动切换二进制帧或文本流)
void Protocol_OutputFrame(Timebase_t tb, const AcqPoint_t *points, uint16_t count,
                          float ch1_v, float ch2_v, float ch2_vpp, uint8_t dig_mask, uint32_t ovr_err)
{
    if (s_current_mode == PROTOCOL_MODE_BINARY) {
        Protocol_SendBinaryFrame(tb, points, count);
    } else {
        Protocol_PrintDebugText(tb, ch1_v, ch2_v, ch2_vpp, dig_mask, ovr_err);
    }
}

void DMA2_Stream6_IRQHandler(void) { HAL_DMA_IRQHandler(&s_tx_dma); }

void DMA2_Stream7_IRQHandler(void) { HAL_DMA_IRQHandler(&s_vcp_tx_dma); }
