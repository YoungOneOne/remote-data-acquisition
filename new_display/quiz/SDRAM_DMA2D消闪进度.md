# SDRAM + DMA2D 消闪方案（当前进度）

> 目标：照抄 `D:\qianrushi\F746_Display` 的做法，用 **SDRAM 离屏缓冲 + DMA2D 整块搬运** 彻底消除闪烁。
>
> **现在请先烧录验证 SDRAM 能不能用**，这一步决定后面走哪条路。

---

## 一、这次改了什么

| 文件 | 改动 |
|---|---|
| `Drivers/.../Src/stm32f7xx_hal_dma2d.c` | **新增**（从 F746_Display 复制）|
| `Drivers/.../Src/stm32f7xx_ll_dma2d.c` | **新增**（同上）|
| `Drivers/.../Inc/stm32f7xx_hal_dma2d.h` | **新增**（同上）|
| `Drivers/.../Inc/stm32f7xx_ll_dma2d.h` | **新增**（同上）|
| `Core/Inc/stm32f7xx_hal_conf.h` | 启用 `HAL_DMA2D_MODULE_ENABLED` |
| `MDK-ARM/test1.uvprojx` | 加入 `stm32f7xx_hal_dma2d.c`（已备份为 `.bak`）|
| `Core/Src/bsp_disco.c` | SDRAM 改用 **SDCLK=HCLK/2 (108MHz)** + 刷新率 `0x0603`（照抄 F746_Display）|
| `Core/Src/main.c` | 开机加 SDRAM 自检 + `printf` 打印结果 |

---

## 二、为什么改 SDRAM 配置

对比两个工程，**时序参数几乎完全一样，只有两处不同**：

| 参数 | F746_Display（实测可用） | quiz（之前，有问题的） |
|---|---|---|
| `SDClockPeriod` | **`CLOCK_PERIOD_2`** → 108 MHz | `CLOCK_PERIOD_3` → 72 MHz |
| 刷新计数 | **`0x0603`** = 1539 | `0x0451` = 1105 |

之前把时钟**降**到 72 MHz 想让它更稳，反而更不对。现在照抄能用的那份。

> 理论上 SDCLK=108MHz 时按 ST 公式刷新计数应该是 1667（0x683），
> 但 F746_Display 用 1539 实测正常。刷新更频繁只会更稳，不会出错。

---

## 三、★ 现在请你做的：烧录并看输出

### 3.1 编译

打开 `D:\qianrushi\quiz\MDK-ARM\test1.uvprojx`：

1. 可能会提示工程文件被外部修改 → **选 Yes / 重新加载**
2. `Project → Rebuild all target files`
3. 预期：**0 Error**

如果报错，重点看这几类：

| 错误 | 处理 |
|---|---|
| `cannot open source input file "stm32f7xx_hal_dma2d.h"` | 头文件路径没加，见 3.4 |
| `Undefined symbol HAL_DMA2D_xxx` | `dma2d.c` 没加进工程（已自动加了，若提示说明要重新打开工程）|
| 其它 | 把错误发我 |

### 3.2 烧录 + 打开 printf 窗口

1. `Flash → Download`（F8）
2. **`View → Serial Windows → Debug (printf) Viewer`** ← 必须开这个，`printf` 走 SWO 不是串口

### 3.3 看输出

**情况 A：成功（我们想要的）**

```
boot: SDRAM OK (C0000000)
boot: touch OK, id 0x54
```

→ 告诉我「SDRAM OK」，我立刻做离屏缓冲 + DMA2D 的消闪方案。

**情况 B：失败**

```
boot: SDRAM FAIL -> fallback to SRAM
boot: touch OK, id 0x54
```

→ 屏幕仍能正常显示（自动退回内部 SRAM 单缓冲），告诉我「SDRAM FAIL」，我改走纯软件优化路线。

**情况 C：死机 / 屏幕全白 / LED 只闪一下就停**

→ SDRAM 访问把总线挂住了。**立刻断电**，告诉我现象，我把自检去掉退回安全版本。

### 3.4 如果报找不到头文件

`Options for Target → C/C++ → Include Paths` 里确认有：

```
..\Drivers\STM32F7xx_HAL_Driver\Inc
```

一般本来就有，不需要动。

---

## 四、后续方案（等你结果）

### 如果 SDRAM OK —— 完整复刻 F746_Display

参考工程的做法（`app_display_ui.c` 第 410~534 行）：

```c
/* 离屏缓冲放 SDRAM（避开帧缓冲区域）*/
#define WAVE_OFFSCREEN_BUF  ((uint16_t *)0xC0080000)   /* FB0/FB1 之后 */

static void draw_waveform(void)
{
    /* 1. 32 位快速清零离屏缓冲（480x207 ≈ 0.1ms）*/
    uint32_t *p32 = (uint32_t *)WAVE_OFFSCREEN_BUF;
    for (i = 0; i < WAVE_W * WAVE_H / 2; i++) p32[i] = 0;

    /* 2. 在离屏缓冲里画网格 + 所有波形（屏幕上看不到）*/
    ...

    /* 3. 一次 DMA2D 整块搬上屏（几十微秒）*/
    HAL_DMA2D_Start(&hdma2d, (uint32_t)WAVE_OFFSCREEN_BUF,
                    (uint32_t)&FB[WV_Y * SCR_W], WAVE_W, WAVE_H);
    HAL_DMA2D_PollForTransfer(&hdma2d, 10);
}
```

**屏幕永远只看到"搬完的完整一帧"，没有任何空窗期 → 零闪烁。**

### 如果 SDRAM 不行 —— 纯软件优化

1. 擦除和绘制合并成一次遍历（省一半时间）
2. 波形区逐列差分：只重画新旧值不一致的列
3. 减小每帧工作量，缩短闪烁窗口

效果会明显改善，但做不到完全零闪。

---

## 五、附：F746_Display 的关键代码位置

供对照查看：

| 内容 | 文件 | 行号 |
|---|---|---|
| 离屏缓冲地址 | `Core/Src/app_display_ui.c` | 54 |
| 离屏画线（Bresenham）| 同上 | 364~404 |
| ★ 核心渲染 + DMA2D 搬运 | 同上 | 410~534 |
| SDRAM 初始化 | `Core/Src/fmc.c` | 87~130 |
| SDRAM 上电序列 | 同上 | 25~65 |
| LTDC 配置（FB 在 SDRAM）| `Core/Src/ltdc.c` | 71 |
