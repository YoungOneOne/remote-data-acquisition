# ADC → 屏幕 demo 使用说明

> 目标：读 ADC 的值，**要么显示成数字，要么变成一条会动的线**。
> 加上：**怎么在不同 `.c` 文件之间传参数**。
>
> 硬件：本工程已经配好 **ADC3 / 通道 0 / PA0**（12 位，0..4095，连续转换）。
> 你要做的只有 3 步：**开 Start → 取平均 → 画到屏上**。

---

## 0. 三分钟总览

```
main.c 主循环
   │
   ├── ADC_Get()          ← 读一次 ADC，返回 0..4095   （新写的文件）
   │
   └── ui_draw(&w)
         └── ui_main()    ← 每帧调一次，从 g_adc 里取最新值画出来
               ├── ui_field(...)             数字
               └── ui_line / ui_hline        线
```

关键点：**读 ADC 在 `main.c`，画屏在 `ui_main()`**，中间靠**一个全局变量**传递。

---

## 1. ADC 现在是什么状态

打开发电机生成的 `Core/Src/adc.c`，能看到已经配好了：

```c
hadc3.Instance              = ADC3;
hadc3.Init.ClockPrescaler   = ADC_CLOCK_SYNC_PCLK_DIV4;
hadc3.Init.Resolution       = ADC_RESOLUTION_12B;      /* 0..4095 */
hadc3.Init.ContinuousConvMode = ENABLE;                /* 连续转换，不用每次触发 */
hadc3.Init.NbrOfConversion  = 1;
...
sConfig.Channel    = ADC_CHANNEL_0;
sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
```

GPIO 也配好了：**PA0 = ADC3_IN0，模拟模式**。

> ⚠ **但是缺了一步**：`main.c` 里只调了 `MX_ADC3_Init()`，
> **没有调 `HAL_ADC_Start()`**，所以 ADC 实际上没在转换，读出来永远是 0。
> 这是最常踩的坑，先解决它。

### 1.1 补上 Start

在 `main.c` 里找这一段（第 73 行附近）：

```c
  MX_ADC3_Init();
```

在 `MX_ADC3_Init();` **后面**、`USER CODE BEGIN 2` 里加：

```c
  /* USER CODE BEGIN 2 */
  if (HAL_ADC_Start(&hadc3) != HAL_OK)      /* ★ 启动 ADC，只调一次 */
  {
    Error_Handler();
  }
  ...
```

`hadc3` 在 `adc.h` 里有 `extern` 声明，直接能用。

> 因为配置成了 **ContinuousConvMode = ENABLE**，`HAL_ADC_Start()` 调完它会自己
> 一直转换，不用在循环里反复 start。每次 `HAL_ADC_GetValue()` 拿的就是"最新一次"结果。

### 1.2 采样时间调长一点（强烈建议）

现在的 `ADC_SAMPLETIME_3CYCLES` 太短。如果你接的是**电位器**（输出阻抗高），
3 个周期采样不足以给采样电容充电，读数会严重偏低 / 乱跳。

在 `adc.c` 里改一行：

```c
sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;   /* 3 -> 480，电位器必改 */
```

> 3 周期的好处是快，适合低阻抗源。**旋钮/电位器一律用 480。**

---

## 2. 写一个 ADC 读取函数（新建 `adc_read.c`）

建两个文件：

- `Core/Src/adc_read.c`
- `Core/Inc/adc_read.h`

**记得在 Keil 里把 `adc_read.c` 加进工程**（`Project` 窗口 → 右键 `Application/User/Core` → `Add Existing Files`），否则编译会报 `undefined symbol`。

### 2.1 `adc_read.h`

```c
#ifndef __ADC_READ_H
#define __ADC_READ_H

#include <stdint.h>

/* 读一次 ADC3_IN0，返回 0..4095（单次）*/
uint16_t ADC_Get(void);

/* 读 16 次取平均，抖动小很多。建议用这个 */
uint16_t ADC_GetAvg(void);

/* 码值 -> 毫伏（0..4095 -> 0..3300）*/
int32_t  ADC_ToMv(uint16_t code);

/* 码值 -> 屏幕 y 坐标（0..4095 -> y_bottom..y_top，自动反过来）*/
int      ADC_ToY(uint16_t code, int y_top, int y_bottom);

#endif
```

### 2.2 `adc_read.c`

```c
#include "main.h"
#include "adc.h"
#include "adc_read.h"

/* --------------------------------------------------------------------------
 *  读一次。ContinuousConvMode 已经开着，所以这里只取值，不用 start
 *  （如果以后把 ContinuousConvMode 改成 DISABLE，就要在取值前
 *    HAL_ADC_Start(&hadc3) + 等 EOC + HAL_ADC_Stop(&hadc3)）
 * -------------------------------------------------------------------------- */
uint16_t ADC_Get(void)
{
    return (uint16_t)HAL_ADC_GetValue(&hadc3);
}

/* --------------------------------------------------------------------------
 *  读 16 次取平均：抖动小、代码短、不用浮点
 *  16 是 2 的幂，右移 4 位就是除 16，比除法快
 * -------------------------------------------------------------------------- */
uint16_t ADC_GetAvg(void)
{
    uint32_t sum = 0;
    int      i;

    for (i = 0; i < 16; i++)
        sum += HAL_ADC_GetValue(&hadc3);

    return (uint16_t)(sum >> 4);
}

/* 码值 -> 毫伏：mV = code * 3300 / 4095 */
int32_t ADC_ToMv(uint16_t code)
{
    return (int32_t)code * 3300 / 4095;
}

/* 码值 -> y 坐标：注意屏幕上 y 越小越高，所以要反过来减 */
int ADC_ToY(uint16_t code, int y_top, int y_bottom)
{
    int32_t span = y_bottom - y_top;
    int     y    = y_bottom - (int32_t)code * span / 4095;

    if (y < y_top)    y = y_top;         /* 夹住，别画到区域外面 */
    if (y > y_bottom) y = y_bottom;

    return y;
}
```

> `ADC_ToY()` 那个"反过来减"是整个 demo 最容易搞错的地方：
> **ADC 值越大越快接近电源，屏幕上就应该越高，而屏幕上 y 越小才越高。**

---

## 3. Demo A：把 ADC 值显示成数字

### 3.1 先定义全局变量（跨文件传参的关键）

**新建 `Core/Inc/app_data.h`**（只有头文件，不需要 `.c`）：

```c
#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

/* ==========================================================================
 *  全局数据区：所有 .c 文件共享的数据都放这里
 *  ★ 头文件里只写 extern 声明，真正的变量定义放在某一个 .c 里
 * ========================================================================== */

extern volatile uint16_t g_adc_code;    /* ADC 原始码值 0..4095 */
extern volatile int32_t  g_adc_mv;      /* 电压，毫伏 */

#endif
```

**在 `adc_read.c` 里定义**（一个变量只能定义一次，其它文件用 `extern`）：

```c
/* 定义在这里，其它文件通过 app_data.h 的 extern 访问 */
volatile uint16_t g_adc_code = 0;
volatile int32_t  g_adc_mv   = 0;
```

> `volatile` 很重要：这个变量在 `main.c` 里被改、在 `ui_demo.c` 里被读，
> 编译器可能把读操作优化成只读一次（拿不到新值）。加 `volatile` 就不会。

### 3.2 `main.c` 里每帧更新一次

在 `while (1)` 里，**刷屏之前**加：

```c
#include "adc_read.h"
#include "app_data.h"
...
while (1)
{
    /* ★ 读 ADC，每轮更新一次全局变量 */
    g_adc_code = ADC_GetAvg();
    g_adc_mv   = ADC_ToMv(g_adc_code);

    ... /* 触摸处理、刷屏那些原来的代码 */
}
```

> 放在 `while` 最前面就行，不用加延时 —— 主循环本来就被 40ms 的刷屏节奏卡住了，
> 所以实际大约 25 次/秒。

### 3.3 `ui_demo.c` 里画成数字

打开 `Core/Src/ui_demo.c`，在 `ui_main()` 里写：

```c
#include "app_data.h"          /* ★ 加在文件顶部 */

static void ui_main(void)
{
    char    buf[24], *p;
    uint8_t changed;

    /* ---- ① 静态标签：只画一次，不用每帧擦 ---- */
    static int inited;
    if (!inited)
    {
        inited = 1;
        ui_fill(0, 0, 480, 272, C_BG);      /* 铺个底色，不然是花屏 */
        ui_text16(20, 40, "ADC3 IN0 (PA0)", C_WHITE, UI_BG_NONE);
    }

    /* ---- ② 电压：用 ui_field，值不变就不重画（不会闪）---- */
    p = buf;
    p = ui_fix(p, g_adc_mv, 3);             /* 1650 -> "1.650" */
    p = ui_str(p, " V");
    ui_field(0, 20, 70, 160, 18, buf, C_CH1, C_BG);

    /* ---- ③ 原始码值 ---- */
    p = buf;
    p = ui_str(p, "code = ");
    p = ui_itoa(p, g_adc_code);
    ui_field(1, 20, 95, 160, 18, buf, C_WHITE, C_BG);

    /* ---- ④ 横向滚动条：直观看到大小 ---- */
    {
        int w = (int)((uint32_t)g_adc_code * 400u / 4095u);   /* 0..400 px */

        ui_rect (20, 125, 400, 16, C_EDGE);                   /* 外框 */
        ui_fill (21, 126, w,    14, C_GREEN);                 /* 填充 */
    }
}
```

### 3.4 为什么数字用 `ui_field` 而不是 `ui_text`

裸帧缓冲没有背景重绘。数值从 `"1.650"` 变成 `"1.65"` 时，**最后一个 `0` 会留在屏幕上**。

`ui_field()` 会记住上次画的内容，**变了才擦掉重画，没变一个像素都不碰** —— 既不残影也不闪。
这就是它存在的理由，数值一律用它。

`ui_field` 的第一个参数是 `id`，取值 `0 ~ 7`（`UI_FIELD_MAX`），**每块数字域用不同的 id**，互不干扰。

---

## 4. Demo B：把 ADC 值变成一条会动的线

这是示波器里那种"电平指示线"。两种画法：

### 4.1 画法一：一根横线（最简单，适合当电平表）

```c
/* 波形区左右边界。ui_layout.h 里没有现成的 WV_X0/WV_X1 宏，
   直接用屏幕坐标 20 ~ 460 就行（留点边）*/
#define X0  20
#define X1  460

static void ui_main(void)
{
    static int y_old = -1;                  /* ★ 必须 static，记住上一帧画在哪 */
    int y = ADC_ToY(g_adc_code, ANA_Y + 4, ANA_BOT - 4);

    /* ① 先擦上一帧的线（用背景色盖掉）*/
    if (y_old >= 0)
        ui_hline(X0, X1, y_old, C_BG);

    /* ② 再画这一帧的线 */
    ui_hline(X0, X1, y, C_CH1);

    /* ③ 记下来，下一帧擦 */
    y_old = y;

    /* 顺手把数值也写出来（会跟着线一起动）*/
    {
        char buf[24], *p = buf;
        p = ui_fix(p, g_adc_mv, 3);
        p = ui_str(p, " V");
        ui_field(0, 20, 30, 160, 18, buf, C_CH1, C_BG);
    }
}
```

`ANA_Y`（= 20，模拟区顶）和 `ANA_BOT`（= 176，模拟区底）在 `ui_layout.h` 里都有。
x 方向没有专门的宏，**直接用字面量 20 和 460**。

**核心就是"先擦旧的、再画新的"**，两步缺一个就会留下一条条残影。

### 4.2 画法二：带刻度的电平表（好看一点）

```c
static void ui_main(void)
{
    static int y_old = -1;
    static int inited;
    int y, i;
    int top = ANA_Y + 6, bot = ANA_Y + ANA_H - 6;

    /* ---- 静态部分只画一次 ---- */
    if (!inited)
    {
        inited = 1;
        ui_fill(0, 0, 480, 272, C_BG);
        ui_text16(20, 0, "ADC LEVEL", C_WHITE, UI_BG_NONE);

        /* 刻度线 + 刻度字：0V / 1.65V / 3.3V */
        for (i = 0; i <= 2; i++)
        {
            int yy = ADC_ToY((uint16_t)(i * 2047), top, bot);
            ui_line_dashed(20, yy, 440, yy, 4, 4, C_GRID);
        }
        ui_text(444, top - 6, "3.3", C_DIM, UI_BG_NONE);
        ui_text(444, bot - 6, "0.0", C_DIM, UI_BG_NONE);
    }

    /* ---- 动态部分：擦旧线 + 画新线 ---- */
    y = ADC_ToY(g_adc_code, top, bot);

    if (y_old >= 0)
        ui_hline(20, 440, y_old, C_BG);     /* 擦 */

    ui_hline  (20, 440, y,   C_CH1);        /* 画 */
    ui_line_thick(20, y, 26, y, 7, C_CH1);  /* 左端加个"方块"当指示头 */

    y_old = y;
}
```

### 4.3 画法三：一整条历史曲线（不擦，让它往左滚）

想画"最近 N 次的走势"，就不要每帧擦，而是每帧把整条曲线重画一遍：

```c
#define HIST 120                            /* 120 个历史点 = 120 列 */

static uint16_t s_hist[HIST];               /* 环形缓冲 */
static int      s_hist_idx;

static void ui_main(void)
{
    int i, prev_y = -1;
    int top = ANA_Y + 6, bot = ANA_Y + ANA_H - 6;

    /* ① 存一个新点（只 push，不擦）*/
    s_hist[s_hist_idx] = g_adc_code;
    s_hist_idx = (s_hist_idx + 1) % HIST;

    /* ② 整块区域清一次（这样不用算哪根线该擦）*/
    ui_fill(20, top, 400, bot - top, C_BG);

    /* ③ 从最老的画到最新的 */
    for (i = 0; i < HIST; i++)
    {
        int idx = (s_hist_idx + i) % HIST;
        int x   = 20 + i * 400 / HIST;
        int y   = ADC_ToY(s_hist[idx], top, bot);

        if (prev_y >= 0)
            ui_line(x - 400 / HIST, prev_y, x, y, C_CH2);

        prev_y = y;
    }
}
```

- 好处：不用管擦，代码简单
- 代价：每帧重画 120 段线（`ui_fill` 一块 + 120 次 `ui_line`），216 MHz 下完全够
- 数据一上电全是 0，曲线会从底部爬起来，正常

---

## 5. 不同 `.c` 文件之间怎么传参数（重点）

这是嵌入式工程里最容易乱的地方。规则只有三条：

### 5.1 三条规则

| 规则 | 说明 |
|---|---|
| **① 变量定义写在 `.c` 里，声明写在 `.h` 里** | 定义 = 真正分配内存那一份，全工程**只能有一个** |
| **② 要给别人用的，`.h` 里加 `extern`** | 别的 `.c` `#include` 这个 `.h` 就能用 |
| **③ 函数同理：`.c` 里写实现，`.h` 里写原型** | 用的人只 `#include` 头文件 |

一句话：**头文件是"菜单"，`.c` 是"厨房"**。别人看菜单点菜，不用进厨房。

### 5.2 错误示范（新手 90% 踩这个）

```c
/* ❌ 错误：在头文件里写 int x = 0; */
/* app_data.h */
int x = 0;          /* 每个 include 它的 .c 都会定义一份 -> 链接报错 
                       multiple definition of `x' */
```

```c
/* ✅ 正确 */
/* app_data.h */
extern int x;       /* 只是声明："有这么个东西，别处有" */

/* app_data.c */
int x = 0;          /* 真正的定义，只有这一处 */
```

### 5.3 本工程推荐的组织方式

我建议把 `ADC值`、`按键值`、`触摸坐标` 这些"到处都要用"的东西，统一放一个头文件：

```c
/* ===================== Core/Inc/app_data.h ===================== */
#ifndef __APP_DATA_H
#define __APP_DATA_H
#include <stdint.h>

/* 数据（定义在 adc_read.c / 别的地方，这里只声明）*/
extern volatile uint16_t g_adc_code;
extern volatile int32_t  g_adc_mv;
extern volatile uint8_t  g_key_event;      /* 1=长按 2=双击 3=单击 */
extern volatile uint8_t  g_run;            /* 1=运行 0=暂停 */

/* 想跨文件调用的函数，也在这里声明 */
void App_Update(void);                      /* main.c 每轮调一次 */

#endif
```

```c
/* ===================== Core/Src/adc_read.c ===================== */
#include "app_data.h"
#include "adc_read.h"

volatile uint16_t g_adc_code = 0;           /* ★ 定义（全工程唯一一份）*/
volatile int32_t  g_adc_mv   = 0;

void App_Update(void)
{
    g_adc_code = ADC_GetAvg();
    g_adc_mv   = ADC_ToMv(g_adc_code);
}
```

```c
/* ===================== Core/Src/main.c ===================== */
#include "app_data.h"
...
while (1)
{
    App_Update();                           /* 一行搞定，细节都在 adc_read.c */
    ...
}
```

```c
/* ===================== Core/Src/ui_demo.c ===================== */
#include "app_data.h"                       /* ★ 只要 include 就能用 */
...
ui_fix(p, g_adc_mv, 3);
```

### 5.4 六种传参方式对照

| 场景 | 怎么写 | 例子 |
|---|---|---|
| **一个值给另一个文件用** | 全局 + `extern` | 上面 `g_adc_mv` |
| **一个值给另一个文件改** | 全局 + `extern` + `volatile` | `g_key_event` |
| **调另一个文件的函数** | 头文件里写原型，`.c` 里实现 | `ADC_GetAvg()` |
| **函数要返回值** | 正常 `return`，原型要一致 | `uint16_t ADC_Get(void)` |
| **函数要一次给多个值** | 传指针（`&变量`），函数里 `*p = ...` | `void ui_measure(&w, &m)` |
| **函数要"回调"你** | 传函数指针 | `ui_set_custom(ui_main)` |
| **数组给别的文件用** | `extern uint16_t buf[];`（**不写长度**） | 见下 |

**指针传参（一次给多个值）示例**：

```c
/* adc_read.h */
void ADC_GetBoth(uint16_t *code, int32_t *mv);   /* 原型用指针 */

/* adc_read.c */
void ADC_GetBoth(uint16_t *code, int32_t *mv)
{
    if (code) *code = ADC_GetAvg();              /* 判空，防止传 NULL */
    if (mv)   *mv   = ADC_ToMv(*code);
}

/* main.c 里调用：把地址传进去 */
uint16_t c;
int32_t  m;
ADC_GetBoth(&c, &m);                             /* ★ 要写 & */
```

**数组跨文件**：

```c
/* 定义（在 .c 里，要写长度）*/
uint16_t s_hist[120];

/* 声明（在 .h 里，不写长度）*/
extern uint16_t s_hist[];
```

### 5.5 三个必踩的坑

| 现象 | 原因 | 解法 |
|---|---|---|
| `undefined symbol xxx` | 函数在 `.c` 里，但那个 `.c` **没加进 Keil 工程** | Project 窗口右键 → `Add Existing Files` |
| `multiple definition of xxx` | 变量定义写在了 `.h` 里 | `.h` 只留 `extern`，定义搬到某个 `.c` |
| 变量读到的一直是旧值 | 别的文件/中断在改，自己没加 `volatile` | 声明和定义**都**加 `volatile` |

> 头文件还要有 `#ifndef / #define / #endif` 保护（上面每个 `.h` 都写了），
> 否则同一个 `.c` include 两次会重复定义。

### 5.6 头文件包含链条（别写成环）

```
app_data.h  ── 不含任何自己的头文件，最干净
adc_read.h  ── 只 include <stdint.h>
adc_read.c  ── include: main.h, adc.h, adc_read.h, app_data.h
main.c      ── include: main.h, adc_read.h, app_data.h, ui_render.h ...
ui_demo.c   ── include: main.h, ui_render.h, ui_layout.h, app_data.h
```

规则：

- **`.h` 里尽量少 include**，能只写 `extern` 就别 include 整个头
- **`.c` 里想 include 多少都行**
- **不要 `a.h` include `b.h`、`b.h` 又 include `a.h`**，会编译不过或出怪错

---

## 6. 完整最小骨架（照抄就能跑）

### 6.1 `main.c`

```c
/* USER CODE BEGIN Includes */
#include "app_data.h"
#include "adc_read.h"
#include "key.h"
#include "bsp_tim.h"
#include "ui_render.h"
#include "ui_demo.h"
#include "touch.h"
/* USER CODE END Includes */

...
static Touch_State s_ts;
static uint8_t     s_touch_ok;

int main(void)
{
    ...
    MX_ADC3_Init();
    ...
    /* USER CODE BEGIN 2 */
    bsp_tim_init();
    Dbg_Init();

    if (HAL_ADC_Start(&hadc3) != HAL_OK)     /* ★ ADC 必须 Start */
        Error_Handler();

    ui_init();
    ui_chrome_enable(0);
    g_ui.ch_mask   = 0;
    g_ui.show_meas = 0;
    g_ui.trig_on   = 0;
    ui_demo_init();

    s_touch_ok = (Touch_Init() == TOUCH_OK) ? 1u : 0u;
    /* USER CODE END 2 */

    while (1)
    {
        App_Update();                        /* ★ 读 ADC -> 全局变量 */

        if (s_touch_ok)
        {
            Touch_Poll(&s_ts);
            ui_touch_set(s_ts.pressed ? (int)s_ts.x : -1,
                         s_ts.pressed ? (int)s_ts.y : -1,
                         (int)s_ts.pressed);
        }

        {
            static uint32_t      frame   = 0;
            static uint32_t      last_ui = 0;
            static wave_window_t w;

            if (HAL_GetTick() - last_ui >= 40)
            {
                last_ui = HAL_GetTick();
                frame++;
                ui_demo_fill(&w, frame);
                ui_draw(&w);                 /* 内部调 ui_main() */
            }
        }
    }
}
```

### 6.2 `ui_demo.c` 的 `ui_main()`

数字 + 电平线一起。`ui_demo.c` 顶部**已有的** include 不用动
（`main.h` / `ui_render.h` / `ui_layout.h` 都在里面了），只要**再补两个**：

```c
/* ui_demo.c 顶部，加在原有 include 后面 */
#include "app_data.h"        /* g_adc_code / g_adc_mv */
#include "adc_read.h"        /* ADC_ToY() */
```

然后 `ui_main()` 写成：

```c
static void ui_main(void)
{
    static int inited;
    static int y_old = -1;
    char buf[24], *p;
    int  y;
    int  top = ANA_Y + 6, bot = ANA_BOT - 6;      /* ANA_BOT = 176 */

    /* ---- 静态背景：只画一次 ---- */
    if (!inited)
    {
        inited = 1;
        ui_fill(0, 0, 480, 272, C_BG);
        ui_text16(20, 2, "ADC3 IN0  PA0", C_WHITE, UI_BG_NONE);
        ui_hline(20, 460, 22, C_EDGE);
        ui_line_dashed(20, top, 460, top, 4, 4, C_GRID);
        ui_line_dashed(20, bot, 460, bot, 4, 4, C_GRID);
    }

    /* ---- 数字：电压 + 原始码值 ---- */
    p = buf;
    p = ui_fix(p, g_adc_mv, 3);
    p = ui_str(p, " V  (");
    p = ui_itoa(p, g_adc_code);
    p = ui_str(p, ")");
    ui_field(0, 20, 30, 300, 18, buf, C_CH1, C_BG);

    /* ---- 动态横线 ---- */
    y = ADC_ToY(g_adc_code, top, bot);

    if (y_old >= 0)
        ui_hline(20, 460, y_old, C_BG);      /* 擦旧 */

    ui_hline(20, 460, y, C_CH1);             /* 画新 */
    ui_line_thick(20, y, 26, y, 7, C_CH1);   /* 指示头 */

    y_old = y;
}
```

---

## 7. 常见问题

| 现象 | 原因 | 解法 |
|---|---|---|
| **读数一直是 0** | 没调 `HAL_ADC_Start()` | 见 §1.1 |
| **读数一直 4095** | PA0 没接东西、悬空，被拉到 VDD | 接电位器中点或分压 |
| **读数乱跳 ±几十** | 采样时间太短 | `ADC_SAMPLETIME_480CYCLES`（§1.2） |
| **数值上升电压，线往下走** | y 轴反了 | 用 `ADC_ToY()`，它内部已经反过来 |
| **数字变了后面留残影** | 用了 `ui_text` 而不是 `ui_field` | 改成 `ui_field`（§3.4） |
| **线上一条条拖影** | 只画新的没擦旧的 | 先 `ui_hline(...,y_old,C_BG)`（§4.1） |
| **屏幕全黑，什么都没有** | `ui_chrome_enable(0)` 关了框架，且没铺底色 | `ui_fill(0,0,480,272,C_BG)` |
| **`undefined symbol ADC_GetAvg`** | `adc_read.c` 没加进 Keil 工程 | Project 窗口 `Add Existing Files` |
| **`multiple definition of g_adc_code`** | 定义写进 `.h` 了 | `.h` 改 `extern`，定义搬到 `.c` |
| **改了 `c` 文件没反应** | 编译缓存 | Keil `Project → Rebuild all` |

---

## 8. 一句话总结

```c
/* 读：main.c 里 */
g_adc_code = ADC_GetAvg();

/* 画数字：ui_main() 里 */
p = ui_fix(p, ADC_ToMv(g_adc_code), 3);
ui_field(0, x, y, w, h, buf, C_CH1, C_BG);

/* 画线：ui_main() 里，先擦旧的再画新的 */
int y = ADC_ToY(g_adc_code, top, bot);
if (y_old >= 0) ui_hline(x0, x1, y_old, C_BG);
ui_hline(x0, x1, y, C_CH1);
y_old = y;

/* 跨文件：.h 里 extern，某个 .c 里定义，谁用谁 include */
```

**记住三件事**：

1. **ADC 要先 `HAL_ADC_Start()`**，否则永远是 0
2. **数字用 `ui_field`，线要"先擦后画"**，不然一个留残影一个拖影
3. **变量定义在 `.c`，声明（`extern`）在 `.h`**，改的变量加 `volatile`
