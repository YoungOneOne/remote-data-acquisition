# new_display 与当前采集端的联通适配

当前代码已简化并删除光标测量。最新操作见 UI_GUIDE.md，两端流程见根目录 CODE_GUIDE.md；下文保留原适配的协议与接线说明。

## 工程与固件

- Keil工程：`quiz/MDK-ARM/test1.uvprojx`
- 生成固件：`quiz/MDK-ARM/test1/test1.hex`
- 原队友版本已保存为Git提交 `1fe5fa0`。
- 仅适配本目录；未覆盖原Display目录中已有的修改，也未改变采集端协议。

当前本机Keil安装ARMCC 5.06，工程已从未安装的ARMCLANG 6.23切换为ARMCC 5.06。HardFault诊断处理保留原ARMCLANG分支，并增加ARMCC5汇编分支。换到队友电脑如需ARMCLANG，可在Keil中重新选择其已安装编译器版本。

## 接线

两块板使用USART6，460800、8N1：

| 采集板 | 新显示板 |
|---|---|
| PC6 / TX | PC7 / RX |
| PC7 / RX | PC6 / TX |
| GND | GND |

PC6/PC7为双板数据链路；显示板ST-Link调试串口USART1仍为115200，不能把两个串口的波特率混为一谈。

## 协议

新显示端支持当前采集端的类型02、1513字节、500点帧，替换旧类型01、383字节、125点协议：

`AA 55 | 02 | tb | count LE16 | seq LE32 | 500*(CH1_8bit,CH2_8bit,digital_bits) | XOR | 0D 0A`

count必须等于500；tb为0～4；XOR覆盖偏移2～1509，校验字节偏移1510，末尾CRLF偏移1511～1512。采样间隔分别为200、100、20、10、2微秒，每格50个采样点。协议流缓冲4096字节，UART DMA接收缓冲8192字节，每读取一块就及时解析，保留原循环次数保护和DMA索引取模修复。

波形窗口500点映射到480像素宽波形区，模拟和数字五路同屏。触发配置移入设置页，保留SDRAM初始化/自检。新版界面操作见 UI_GUIDE.md。显示时基取自数据帧，不用新选择的时基重标旧数据。STOP/SINGLE及断线后仍保留最后有效窗口，状态栏在线标志独立更新。

## 反向控制

TB、RUN/STOP、GEN、ARM、SYNC继续使用采集端原命令。触发设置采用一次提交的原子命令：

`TRIG <source> <edge> <mode> <level>\r\n`

source映射：0=CH1、1=CH2、2=DIG2、3=DIG0、4=DIG1；edge为0上升/1下降；mode为0 AUTO/1 NORMAL/2 SINGLE；level为0～255。设置页Source按钮轮换五个源。匹配完整触发确认前，不显示在途旧配置波形；ARM同样等待对应确认，避免旧应答解除新配置的等待。

界面显示D0/D1/D2对应数据掩码bit0/bit1/bit2，即采集端DIG0/DIG1/DIG2；内部宏统一为CHM_DIG0/DIG1/DIG2。SINGLE有效捕获后RUN状态转STOP，ARM可重新启动。

状态栏fps沿用队友程序的有效收帧率统计，不代表已实测到相同的LCD绘制刷新率。两端当前固件配套，采集端需使用WAVE500-TRIGFIX版本（本轮代码简化后两端均已重新编译）。

## 验证

已用本机Keil完整编译工程，再增量编译最终修改。协议测试直接编译运行本目录真实app_proto.c和app_link.c，UART与时钟使用原生测试桩，覆盖五档时基、25种分段组合、连续8帧积压、校验/尾部/点数/时基错误恢复、状态同步、原子触发确认、旧确认拒绝、单次重新启动和冻结窗口。运行：`tools/run_new_display_protocol_test.bat`，依赖本机Visual Studio 2022 C++工具，路径可按另一台电脑调整。

未烧录或进行双板实机测试。将上述test1.hex烧录到显示板，采集端Acquisition/MDK-ARM/Acquisition/Acquisition.hex烧录到采集板，复位后确认能收帧；再验证五档时基、五路同时显示与独立开关、五个触发源、SINGLE/ARM、RUN/STOP与GEN频率控制。

两端共享定义和代码清理说明见根目录 CODE_GUIDE.md。完整软件联通回归运行 `python tools/test_two_board_alignment.py`。
