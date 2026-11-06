# 新版显示端界面

当前版本保留五路波形和自动测量，已删除光标测量及拖动操作。

## 主界面

- 屏幕宽 480 像素，横向十格，每格对应 50 个采样点。
- 上方显示 CH1、CH2，下方三条数字通道显示 D0、D1、D2，五路使用同一帧和同一触发起点。
- 顶栏显示 RUN/STOP、波形实际时基、触发源/模式/边沿/电平和有效接收帧率。
- 底栏始终显示 CH1 平均电压、CH2 频率与峰峰值、三路数字当前电平。
- 八个按钮依次为 STOP/RUN、TIME、CH1、CH2、D0、D1、D2、MENU。

TIME 循环切换五档时基，通道按钮各自控制显示。暂停或断线后保留最后一帧，菜单仍可使用。触摸波形区域没有光标或测量动作。

## 设置页

右上角 BACK 返回。只有三个页签：

| 页签 | 操作 |
| --- | --- |
| TRIGGER | Source、Mode、Edge 循环选择；模拟源可用 +/- 改电平；ARM SINGLE 重新等待单次触发；SYNC STATE 查询状态 |
| OUTPUT | 开关 PB4 方波，选择 100、500、1k、2k、5k、10kHz，约 50% 占空比 |
| MEASURE | 查看五路频率、模拟峰峰值和数字占空比；REFRESH 读取最近一帧；显示丢帧和错误计数 |

数字触发只判断逻辑跳变，电平栏显示 Digital edge。方波输出开关不影响采集；启停由采集端的 START/STOP 确认更新，SYNC 格式没有单独的启停字段。

菜单打开后仍接收数据。测量页使用打开菜单或点击 REFRESH 时保存的一帧，方便阅读；普通波形到达不会让菜单连续重画。数据不足一个完整周期时，频率显示 --。

## 代码与验证

app_main.c 处理接收和触摸，app_ui.c 直接绘制三个设置页，app_screen.c 画顶栏和底栏，app_gfx.c 画网格、波形并计算自动测量。界面配置逐项比较，有变化才刷新。

SDRAM 自检成功时使用三缓冲，失败时使用 SRAM 单缓冲。单缓冲先擦完旧通道，再画全部新通道。

运行 tools\run_new_display_ui_test.bat、tools\run_new_display_protocol_test.bat，或 python tools/test_two_board_alignment.py。

测试覆盖单缓冲/三缓冲、五路独立开关、五档坐标、三个菜单、触摸防穿透、暂停、自动测量、分包和错误恢复。UI_PREVIEW.png 来自真实 C 绘图代码和合成数据，是桌面预览。

本次 Keil 完整编译为 0 错误、0 警告。显示端固件：quiz/MDK-ARM/test1/test1.hex；采集端固件：../Acquisition/MDK-ARM/Acquisition/Acquisition.hex。本次两端代码均有简化，需分别烧录相应板卡。尚未烧录或双板实测，上板检查五档时基、五路显示、触发、方波输出和帧率。

工程共用根目录 Common/scope_protocol.h，复制时保持相对位置。两端流程和算法见 ../CODE_GUIDE.md。
