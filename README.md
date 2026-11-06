# 远程数据采集系统

基于 STM32F746 的双板采集与显示系统，包含当前采集端和新版显示端源码。

## 工程入口

- 采集端：`Acquisition/MDK-ARM/Acquisition.uvprojx`
- 新版显示端：`new_display/quiz/MDK-ARM/test1.uvprojx`
- 共享协议：`Common/scope_protocol.h`

使用 Keil MDK 与 ARM Compiler 5.06，保持目录结构以解析共享头文件和驱动依赖。

## 接线与通信

两板 USART6 使用 460800、8N1，PC6（TX）与对端 PC7（RX）交叉连接，并连接 GND。
支持两路模拟输入、三路数字输入、五档时基、触发控制、方波发生与自动测量。

## 文档

- [代码说明](CODE_GUIDE.md)
- [采集端说明](Acquisition/README.md)
- [显示端适配及接线](new_display/ADAPTATION.md)
- [显示端操作](new_display/UI_GUIDE.md)
- [500点波形协议](WAVEFORM_500_PROTOCOL.md)

`tools/` 包含随工程提供的协议、算法检查与串口辅助工具，部分脚本依赖本机编译器路径或原开发环境。具体以脚本内容为准。

本仓库保存源码快照；不包含旧显示端、课程资料、采样数据和本地构建结果。第三方组件保留其原有版权与许可声明。
