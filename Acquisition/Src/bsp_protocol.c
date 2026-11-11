// 串口双向通信：接收控制命令，发送 500 点波形或调试文本
#include "bsp_protocol.h"
#include "bsp_generator.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define UART_RX_RING_SIZE 256
#define COMMAND_BUFFER_SIZE 64
#define COMMAND_TIMEOUT_MS 100

// 输出模式决定发文本还是二进制，与 AUTO、NORMAL、SINGLE 触发模式分开
static ProtocolMode_t output_mode = PROTOCOL_MODE_BINARY;
static uint32_t frame_sequence; // 每组织一帧波形递增一次

// HAL 每次只接收一个字节，两路串口各用一个接收位置
static uint8_t uart1_rx_byte;
static uint8_t uart6_rx_byte;

// 中断保存接收字节，主循环等波形发送结束后再处理命令
static volatile uint8_t  rx_buffer[UART_RX_RING_SIZE];
static volatile uint16_t rx_write_index; // 中断下一次写入的位置
static volatile uint16_t rx_read_index;  // 主循环下一次读取的位置

// USART1 接上位机，USART6 接显示端，两路使用同一套命令和波形格式
// 接收队列共用，操作时由一端发送完整命令，避免两端字节交错

static void Protocol_ExecuteCommand(const char *cmd);
static void Protocol_SendState(void);

// 串口和 DMA 已由 CubeMX 初始化，这里开启接收
void Protocol_Init(void)
{
    rx_write_index = 0;
    rx_read_index  = 0;

    HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
    HAL_UART_Receive_IT(&huart6, &uart6_rx_byte, 1);
}

ProtocolMode_t Protocol_GetMode(void)
{
    return output_mode;
}

uint8_t Protocol_IsBusy(void)
{
    // 两路发送共用一个缓冲区，任一路没发完都不能填入下一帧
    return huart6.gState != HAL_UART_STATE_READY ||
           huart1.gState != HAL_UART_STATE_READY;
}

// printf 同时输出到电脑串口和双板串口；DMA 忙时不插入文本
int fputc(int ch, FILE *f)
{
    // printf 会逐字节调用这里，同一个字符分别发送给两端
    if (!Protocol_IsBusy())
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
        HAL_UART_Transmit(&huart6, (uint8_t *)&ch, 1, 10);
    }

    return ch;
}

// 收到的字节先入队，再开启下一字节接收
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t *byte;
    uint16_t next;

    if (huart == &huart1)
    {
        byte = &uart1_rx_byte;
    }
    else if (huart == &huart6)
    {
        byte = &uart6_rx_byte;
    }
    else
    {
        // 这里只接收 USART1 和 USART6 的数据
        return;
    }

    // 写到末尾后回到开头，留一个空位置区分队列满和队列空
    next = (rx_write_index + 1) % UART_RX_RING_SIZE;

    if (next != rx_read_index)
    {
        rx_buffer[rx_write_index] = *byte;
        rx_write_index = next;
    }
    // 队列满时不写入这个字节，避免覆盖主循环还没取走的数据

    HAL_UART_Receive_IT(huart, byte, 1);
}

// 接收出错后重新开启字节接收
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        HAL_UART_Receive_IT(huart, &uart1_rx_byte, 1);
    }
    else if (huart == &huart6)
    {
        HAL_UART_Receive_IT(huart, &uart6_rx_byte, 1);
    }
}

// 主循环读取队列，遇到换行执行命令
void Protocol_ProcessRx(void)
{
    // 半条命令可能分几次进入主循环，static 让这些内容保留到下次调用
    static char     command_buffer[COMMAND_BUFFER_SIZE];
    static uint8_t  command_length = 0;
    static uint32_t last_rx_tick   = 0;
    uint8_t         ch;

    // 接收意外停止时重新开启；发送波形期间也要继续接收命令
    if (huart1.RxState == HAL_UART_STATE_READY)
    {
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
    }
    if (huart6.RxState == HAL_UART_STATE_READY)
    {
        HAL_UART_Receive_IT(&huart6, &uart6_rx_byte, 1);
    }

    if (Protocol_IsBusy())
    {
        // 暂不执行命令，接收中断仍会把新字节存进队列
        return;
    }

    // 超过 100ms 未继续处理的半条命令作废
    if (command_length > 0 && (HAL_GetTick() - last_rx_tick > COMMAND_TIMEOUT_MS))
    {
        command_length = 0;
    }

    while (rx_read_index != rx_write_index)
    {
        // 每取出一个字节就移动读位置，直到队列暂时读空
        ch            = rx_buffer[rx_read_index];
        rx_read_index = (rx_read_index + 1) % UART_RX_RING_SIZE;
        last_rx_tick  = HAL_GetTick();

        // CR、LF、CRLF 都支持，空行不执行
        if (ch == '\r' || ch == '\n')
        {
            if (command_length > 0)
            {
                command_buffer[command_length] = '\0';
                // 补上字符串结束符后，才能交给 strcmp 和 sscanf 处理
                Protocol_ExecuteCommand(command_buffer);
                command_length = 0; // 下一次换行若紧跟着到来，就按空行跳过
            }
        }

        else if (command_length < sizeof(command_buffer) - 1)
        {
            command_buffer[command_length++] = (char)ch;
        }
    }
}

// 命令用大写，参数用空格分隔
static void Protocol_ExecuteCommand(const char *cmd)
{
    int          timebase, frequency;
    unsigned int source, edge, mode, level;

    // 带参数的命令先匹配前缀，再由 sscanf 读取参数
    // RUN、STOP 这类不带参数的命令直接比较整条字符串

    if (strncmp(cmd, "TB ", 3) == 0)
    {
        // 先读出档位编号，格式和范围都正确后才修改采样周期
        if (sscanf(cmd + 3, "%d", &timebase) != 1)
        {
            printf("$ERR:TB_FORMAT\r\n");
            return;
        }

        if (timebase < 0 || timebase >= TB_COUNT)
        {
            printf("$ERR:TB_RANGE\r\n");
            return;
        }

        Acquisition_SetTimebase((Timebase_t)timebase);
        printf("$OK:TB=%d\r\n", timebase);
    }

    else if (strcmp(cmd, "RUN") == 0)
    {
        Acquisition_Resume();
        printf("$OK:RUN\r\n");
    }

    else if (strcmp(cmd, "STOP") == 0)
    {
        // STOP 暂停波形输出，不停止串口命令接收
        Acquisition_Pause();
        printf("$OK:STOP\r\n");
    }

    else if (strncmp(cmd, "TRIG ", 5) == 0)
    {
        // 一条命令设置触发源、边沿、模式和阈值
        if (sscanf(cmd + 5, "%u %u %u %u", &source, &edge, &mode, &level) != 4)
        {
            printf("$ERR:TRIG_INVALID\r\n");
            return;
        }

        // 触发源编号见头文件，模式 0、1、2 对应 AUTO、NORMAL、SINGLE
        if (source >= TRIG_SRC_COUNT || edge > 1 || mode > 2 || level > 255)
        {
            printf("$ERR:TRIG_INVALID\r\n");
            return;
        }

        Acquisition_SetTrigger((TrigSource_t)source, (TrigEdge_t)edge, (TrigMode_t)mode,
                               (uint8_t)level);
        Acquisition_Resume(); // 参数设置完后重启一次采样
        printf("$OK:TRIG=%u,%u,%u,%u\r\n", source, edge, mode, level);
    }

    else if (strcmp(cmd, "ARM") == 0)
    {
        // 已经单次触发停止后，ARM 可以重新等待下一次边沿
        Acquisition_ArmSingleTrigger();
        printf("$OK:ARM_SINGLE\r\n");
    }

    else if (strncmp(cmd, "GEN ", 4) == 0)
    {
        if (sscanf(cmd + 4, "%d", &frequency) != 1)
        {
            printf("$ERR:GEN_FORMAT\r\n");
            return;
        }

        if (frequency < GEN_FREQ_MIN || frequency > GEN_FREQ_MAX)
        {
            printf("$ERR:GEN_RANGE\r\n");
            return;
        }

        Generator_SetFrequency((uint32_t)frequency);
        printf("$OK:GEN=%d\r\n", frequency);
    }

    else if (strcmp(cmd, "GEN_START") == 0)
    {
        Generator_Start();
        printf("$OK:GEN_START\r\n");
    }

    else if (strcmp(cmd, "GEN_STOP") == 0)
    {
        Generator_Stop();
        printf("$OK:GEN_STOP\r\n");
    }

    else if (strcmp(cmd, "MODE 0") == 0)
    {
        // 串口助手调试时看文本，不影响实际采样和触发方式
        output_mode = PROTOCOL_MODE_TEXT;
        printf("$OK:MODE_TEXT\r\n");
    }

    else if (strcmp(cmd, "MODE 1") == 0)
    {
        // 显示端正常接收的是这种 500 点二进制波形
        output_mode = PROTOCOL_MODE_BINARY;
        printf("$OK:MODE_BINARY\r\n");
    }

    else if (strcmp(cmd, "SYNC") == 0)
    {
        // 只查询当前状态，未触发或暂停时也能正常回复
        Protocol_SendState();
    }

    else
    {
        printf("$ERR:UNKNOWN_CMD:%s\r\n", cmd);
    }
}

// 回复当前时基、方波和触发状态
static void Protocol_SendState(void)
{
    // RUN 表示是否允许输出，TRGD 表示最近一次触发查找是否找到边沿
    TriggerConfig_t *trig = Acquisition_GetTriggerConfig();

    printf("$STATE,TB:%d,MODE:%d,GEN:%u,RUN:%d,"
           "TRIG:%s,%s,%s,LVL:%u,TRGD:%d\r\n",
           Acquisition_GetTimebase(), output_mode,
           (unsigned int)Generator_GetFrequency(), Acquisition_IsRunning(),
           Acquisition_GetTrigSourceName(trig->source),
           Acquisition_GetTrigEdgeName(trig->edge),
           Acquisition_GetTrigModeName(trig->mode), trig->level, trig->triggered);
}

// 一帧：10 字节帧头 + 500 个三字节点 + 1 字节校验 + CRLF
void Protocol_SendBinaryFrame(Timebase_t tb, const AcqPoint_t *points)
{
    static uint8_t tx_buf[SCOPE_FRAME_BYTES]; // DMA 发送期间函数已返回，缓冲区不能放在局部栈上
    uint16_t       pos = SCOPE_HEADER_BYTES;
    uint16_t       i;
    uint8_t        checksum = 0;

    if (Protocol_IsBusy())
    {
        return;
    }

    // 填写帧头，点数和序号按低字节在前发送
    // 前 10 字节依次为帧头、类型、时基、点数和序号
    tx_buf[0] = SCOPE_HEAD0;
    tx_buf[1] = SCOPE_HEAD1;
    tx_buf[2] = SCOPE_WAVE_TYPE;
    tx_buf[3] = (uint8_t)tb; // 接收端用这个编号确定每点的时间间隔

    // 500 为 0x01F4，先发 F4，再发 01
    tx_buf[4] = (uint8_t)ACQ_FRAME_POINTS;
    tx_buf[5] = (uint8_t)(ACQ_FRAME_POINTS >> 8);

    tx_buf[6] = (uint8_t)frame_sequence;
    tx_buf[7] = (uint8_t)(frame_sequence >> 8);
    tx_buf[8] = (uint8_t)(frame_sequence >> 16);
    tx_buf[9] = (uint8_t)(frame_sequence >> 24);

    frame_sequence++; // 接收端可根据序号判断是否漏帧

    for (i = 0; i < ACQ_FRAME_POINTS; i++)
    {
        // 每点三个字节：CH1、CH2、数字电平，500 点共 1500 字节
        tx_buf[pos++] = points[i].ch1_raw8;
        tx_buf[pos++] = points[i].ch2_raw8;
        tx_buf[pos++] = points[i].dig_mask;
    }

    // 校验从类型字节开始，到最后一个采样点结束，不含 AA55
    for (i = 2; i < pos; i++)
    {
        // 将校验范围内的字节依次异或，接收端再按同一范围计算核对
        checksum ^= tx_buf[i];
    }

    tx_buf[pos++] = checksum;
    tx_buf[pos++] = '\r';
    tx_buf[pos++] = '\n';

    // 两路 DMA 共用发送缓冲区，都发送完后才能改写
    // 启动后由 DMA 发送，主循环返回后继续处理接收和状态判断
    HAL_UART_Transmit_DMA(&huart6, tx_buf, pos);
    HAL_UART_Transmit_DMA(&huart1, tx_buf, pos);
}

// 文本调试流输出 (MODE 0，适合普通串口助手直接查看)
void Protocol_PrintDebugText(Timebase_t tb, float ch1_v, float ch2_v, float ch2_vpp,
                             uint8_t dig_mask, uint32_t ovr_err)
{
    TriggerConfig_t *trig = Acquisition_GetTriggerConfig();
    uint8_t          d0   = (dig_mask & 0x01) ? 1 : 0;
    uint8_t          d1   = (dig_mask & 0x02) ? 1 : 0;
    uint8_t          d2   = (dig_mask & 0x04) ? 1 : 0;

    // 电压、峰峰值和 OVR 主要供串口调试，二进制波形中不额外发送这些字段

    printf("[%s] Trig:[%s %s %s L:%u %s] | "
           "CH1:%1.3fV | CH2:%1.3fV(Vpp:%1.3fV) | "
           "DIG:[D0:%d D1:%d D2:%d] | OVR:%u\r\n",
           Acquisition_GetTimebaseName(tb), Acquisition_GetTrigSourceName(trig->source),
           Acquisition_GetTrigEdgeName(trig->edge),
           Acquisition_GetTrigModeName(trig->mode), trig->level,
           trig->triggered ? "OK" : "WAIT", ch1_v, ch2_v, ch2_vpp, d0, d1, d2, ovr_err);
}
