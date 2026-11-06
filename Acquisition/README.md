# 采集端代码阅读说明

工程入口：MDK-ARM/Acquisition.uvprojx，使用 ARM Compiler 5.06。

先看 Src/main.c。初始化后主循环只做两件事：Protocol_ProcessRx() 接收并执行命令，App_SendFrame() 在串口空闲且波形可用时发送。

PF8 的本地换挡已在修改前被禁用，本次去掉了无效的按键处理，仍保留 PF8 的 DIG1 采样。

## 三个主要文件

| 文件 | 功能 |
| --- | --- |
| Src/bsp_acquisition.c | 两路 ADC、三路数字采样、时基、触发和一屏数据提取 |
| Src/bsp_protocol.c | 收命令、回状态、发波形 |
| Src/bsp_generator.c | PB4 输出 100Hz～10kHz、约 50% 占空比方波 |

## 一帧数据怎么得到

1. TIM2 定时触发 ADC3，DMA 交替保存 CH1、CH2。
2. TIM2 中断读取 PI0、PF8、PI2，保存三路数字值。
3. DMA 每完成 500 点，回调将这一段加入最近 1000 点历史。
4. Copy_Snapshot() 复制历史并检查版本号，避免中断期间数据被改写。
5. Find_Trigger_Index() 逐点找边沿，再取连续 500 点，计算平均电压和 CH2 峰峰值。

Read_TriggerValue() 只负责读选定通道，Check_Trigger_Edge() 只负责比较前后两点。模拟通道判断跨过阈值，数字通道判断 0/1 跳变。

保留 1000 点，是为了让触发点后面仍有一屏数据。版本检查、DMA 缓冲和静态发送缓冲都与并发读写有关，不能只为减少行数而合并。

## 运行控制

- AUTO：有边沿则对齐，没有也输出。
- NORMAL：有边沿才输出。
- SINGLE：触发后输出一帧，随后停止输出。
- ARM：清理历史，重新等待单次触发。
- STOP：暂停输出，底层继续采样。
- RUN：清理历史，恢复输出。
- SYNC：只查询状态。

每屏 500 点。五档采样间隔是 200、100、20、10、2 微秒，对应 10ms、5ms、1ms、500us、100us/div。换挡或修改触发时会重新对齐 ADC 和数字采样缓冲。

## 通信与方波

两路串口均为 460800、8N1；USART6 连接显示板，USART1 连接电脑。波形包固定 1513 字节，两路发送 DMA 共用一份数据，发送结束后才处理控制命令。文本模式用时间差限制输出间隔，主循环不再忙等。

TIM3 计数时钟为 1MHz：ARR = 1000000 / 频率 - 1，CCR1 = (ARR + 1) / 2。频率先限制在 100～10000Hz，再写寄存器。

原有文本命令和二进制协议继续兼容。共享定义在 ../Common/scope_protocol.h，交付时一并保留。

## 验证

运行 python tools/test_acquisition_refactor.py，或运行 python tools/test_two_board_alignment.py 检查两端，再运行 python tools/check_firmware_stack.py 检查构建报告。

算法测试编译原版和当前真实 C 代码，对比五种触发源、双边沿、600 组完整取帧场景、快照更新冲突、DMA 回调和 11001 个方波频率请求；协议测试对比 102 组命令及五档波形包。

本次 Keil 完整构建为 0 错误、0 警告，固件位于 MDK-ARM/Acquisition/Acquisition.hex。未烧录或实板验证；完整两端说明见 ../CODE_GUIDE.md。
