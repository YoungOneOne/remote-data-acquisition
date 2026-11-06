# 屏幕绘制 & 触摸 API 说明（test1 / STM32F746G-DISCO）

> 屏幕：480 × 272，RGB565，帧缓冲在内部 SRAM `0x20010000`

---

## 全部 39 个函数索引（哪个不会用就查哪节）

| # | 函数 | 一句话 | 在哪节 |
|---|---|---|---|
| **框架与清屏（8）** | | | |
| 1 | `ui_init()` | 上电初始化，只调一次 | §0 §7 |
| 2 | `ui_draw(&w)` | 每帧调一次，画波形+你的界面 | §7 |
| 3 | `ui_set_custom(fn)` | 注册你的界面回调，**界面写这里** | §5 |
| 4 | `ui_force_redraw()` | 改了 `g_ui` 后强制重画状态栏/工具条 | §6 |
| 5 | `ui_chrome_enable(on)` | `0` = 关掉框架整屏自己画 | §11.1 |
| 6 | `ui_clear(color)` | 整屏刷色 + 清内部缓存 | §11.2 |
| 7 | `ui_reset()` | 清屏 + 重建网格/读数面板/工具条 | §11.2 |
| 7b | `ui_set_bg_color(c)` | **改整屏背景色**（主题色） | §11.2.5 |
| 7c | `ui_get_bg_color()` | 查当前背景色 | §11.2.5 |
| 8 | `ui_use_sdram()` | **本板不要调**（SDRAM 不通） | §11.5 |
| **颜色与字符串（4）** | | | |
| 9 | `ui_rgb(r,g,b)` | RGB888 → RGB565 | §2 |
| 10 | `ui_str(p, s)` | 拷贝字符串，返回新末尾 | §4 |
| 11 | `ui_itoa(p, v)` | 整数 → 字符串，自带负号 | §4 |
| 12 | `ui_fix(p, v, dec)` | 定点小数，`ui_fix(p,165,2)`→`"1.65"` | §4 |
| **图元（8）** | | | |
| 13 | `ui_px(x, y, c)` | 一个点 | §3 |
| 14 | `ui_fill(x,y,w,h,c)` | 实心矩形 | §3 |
| 15 | `ui_rect(x,y,w,h,c)` | 空心矩形（1px 边框） | §3 |
| 16 | `ui_hline(x0,x1,y,c)` | 水平线 | §3 |
| 17 | `ui_vline(x,y0,y1,c)` | 垂直线 | §3 |
| 18 | `ui_line(x0,y0,x1,y1,c)` | 任意直线 | §3 |
| 19 | `ui_line_thick(...,t,c)` | 粗线 | §3 |
| 20 | `ui_line_dashed(...,on,off,c)` | 虚线 | §3 |
| **文字（6）** | | | |
| 21 | `ui_text(x,y,s,fg,bg)` | Font12 画字，返回宽度 | §4 |
| 22 | `ui_text16(...)` | Font16 画字 | §4 |
| 23 | `ui_text_w(s)` | 量 Font12 宽度 | §4 |
| 24 | `ui_text_w16(s)` | 量 Font16 宽度 | §4 |
| 25 | `ui_text_c(cx,y,s,fg,bg)` | Font12 居中画 | §4 |
| 26 | `ui_text_c16(...)` | Font16 居中画 | §4 |
| **数据刷新（3）** | | | |
| 27 | `ui_text_at(x,y,w,h,s,fg,bg)` | 擦一块再居中画字（每次都重画） | §7.5 |
| 28 | `ui_field(id,...)` | ★内容没变就跳过，刷数值用它 | §7.5 |
| 29 | `ui_field_reset()` | 清 `ui_field` 缓存 | §7.5 |
| **换算与测量（4）** | | | |
| 30 | `ui_code_to_volt(code)` | 12 位码值 → 电压 | §11.3 |
| 31 | `ui_scan_label(scan)` | 0..4 → `"1us"`…`"10ms"` | §11.3 |
| 32 | `ui_scan_div_ns(scan)` | 0..4 → 1000…10000000 ns | §11.3 |
| 33 | `ui_measure(&w, &m)` | 一窗数据 → freq/vpp/vavg/width/duty | §11.4 |
| **触摸（2）** | | | |
| 34 | `ui_toolbar_hit(x,y)` | 屏幕坐标 → 工具条按钮号 | §9 |
| 35 | `ui_touch_set(x,y,down)` | 把触摸位置交给界面 | §9 |
| 35b | `ui_cursor_enable(on)` | 开关触摸十字准星（**默认关**） | §9 |
| 35c | `ui_cursor_is_on()` | 查准星开没开 | §9 |
| **按键控件（4）** | | | |
| 36 | `ui_btn_init(&b)` | 运行时初始化/复位按钮 | §10 |
| 37 | `ui_button(&b,...)` | ★画+判命中+防连发，返回 1 = 被点 | §10 |
| 38 | `ui_btn_colors(&b,...)` | 改配色 | §10 |
| 39 | `ui_btn_invalidate(&b)` | 区域被盖住后强制重画 | §10 |

> 触摸驱动的 `Touch_Init` / `Touch_Scan` / `Touch_Poll` 在 `触摸屏使用说明.md`。

---

## 0. 工程文件结构（先看这个）

| 文件 | 职责 | 你要不要改 |
|---|---|---|
| `Core/Src/main.c` | **只做初始化和主循环**，不含任何界面代码 | 一般不用动 |
| `Core/Src/ui_render.c` / `Inc/ui_render.h` | **绘图引擎 + 框架**：图元、文字、波形、状态栏、工具条、触摸准星、按键控件 | 改布局/加图元时改 |
| `Core/Inc/ui_layout.h` | **布局常量 + 配色表**（屏幕分区、颜色 `C_xxx`）| ★改布局只改这里 |
| `Core/Src/ui_demo.c` / `Inc/ui_demo.h` | 界面内容（5 个测试页）+ 工具条按钮动作 + 演示波形数据 | 换成你自己的界面时改 |
| `Core/Src/touch.c` / `Inc/touch.h` | FT5336 触摸驱动（I2C3，轮询）| 一般不用动 |
| `Core/Src/bsp_disco.c` / `Inc/bsp_disco.h` | LTDC 出图控制 + 背光（+ 停用的 SDRAM 代码）| 不用动 |
| `Core/Src/font12.c` / `font16.c` | 字库数据（ST 原版，95 个 ASCII 字符）| 不用动 |

**数据流：**

```
main 主循环
  ├── get_keynum()   物理按键 PG7   -> 1=长按 2=双击 3=单击 -> 改全局状态   【main.c】
  ├── Touch_Poll()   触摸轮询       -> 拿 x/y/pressed
  ├── ui_touch_set(x, y, pressed)   -> 把触摸位置告诉界面（不调则按钮点不动）
  └── ui_draw(&w)   每 40ms 一次 = 25fps
        ├── 擦上一帧波形
        ├── 画波形 / 触发线 / 读数
        ├── 调 ui_set_custom() 注册的回调  .... ★ 你的界面全写这里（ui_demo.c）
        │      └── ui_button() 在这里创建触摸按键 + 按全局状态画内容
        ├── 画触摸准星
        └── flip()
```

> **两种按键分开写**：物理按键在 `main.c` 主循环里用 `get_keynum()` 读；
> 触摸按键在 `ui_main()` 里用 `ui_button()` 创建。
> 两者都通过全局变量影响界面。详见 `触摸屏使用说明.md` 第 0.7 节。

---

## 1. 坐标系与屏幕分区

```
(0,0) ───────────────────────────── x → 479
  │ ┌──────────────────────────────┐
  │ │ 状态栏          y  0 ~ 19    │  ST_Y=0    ST_H=20
  │ ├──────────────────────────────┤
  │ │ 模拟通道区      y 20 ~ 175   │  ANA_Y=20  ANA_H=156   (6 格 x 26px)
  │ │                              │
  │ ├──────────────────────────────┤  ANA_BOT=176
  │ │ 数字通道区      y176 ~ 226   │  DIG_Y=176 (3 lane x 17px)
  │ ├──────────────────────────────┤  WV_BOT=227
  │ │ 工具条          y227 ~ 271   │  TB_Y=227  TB_H=45
  │ └──────────────────────────────┘
  y
  ↓ 271
```

- 原点在**左上角**，x 向右增大，y 向下增大
- 直接用数字写坐标也行，但建议用 `ui_layout.h` 里的常量（改布局只改那一处）
- 越界的绘制会被自动裁掉，不会跑飞

---

## 2. 颜色

内部是 **RGB565**（16 位：R 5 位 / G 6 位 / B 5 位）。

### 用 RGB 生成

```c
uint16_t c = ui_rgb(255, 0, 0);      /* 纯红 */
uint16_t c = ui_rgb(0, 255, 0);      /* 纯绿 */
uint16_t c = ui_rgb(30, 40, 60);     /* 深灰蓝 */
```

### 用现成常量（`ui_layout.h`）

| 常量 | 颜色 | 常量 | 颜色 |
|---|---|---|---|
| `C_BG` | 黑（波形区底） | `C_GRID` | 暗灰（细网格）|
| `C_GRID_MAJ` | 中灰（中轴/边框格）| `C_EDGE` | 灰（区域边框）|
| `C_CH1` | **黄** | `C_CH2` | **青** |
| `C_D1` | **绿** | `C_D2` | **品红** |
| `C_D3` | **橙** | `C_TRIG` | **红**（触发电平）|
| `C_ST_BG` | 深藏蓝（状态栏底）| `C_ST_TX` | 白（状态栏字）|
| `C_TB_BG` | 深灰蓝（工具条底）| `C_TB_BTN` | 蓝灰（按钮）|
| `C_TB_BTN_ON` | 亮蓝灰（按钮激活）| `C_TB_TX` | 白（按钮字）|
| `C_GREEN` | 亮绿 | `C_AMBER` | 琥珀 |
| `C_RED` | 红 | `C_BLUE` | 亮蓝 |
| `C_WHITE` / `C_BLACK` | 白 / 黑 | `C_DIM` | 变暗文字 |

---

## 3. 图元

| 函数 | 作用 |
|---|---|
| `ui_px(x, y, color)` | 画一个像素 |
| `ui_fill(x, y, w, h, color)` | **实心**矩形，`(x,y)` 是左上角 |
| `ui_rect(x, y, w, h, color)` | **空心**矩形，1 像素边框，内部不动 |
| `ui_hline(x0, x1, y, color)` | 水平线（含两端）|
| `ui_vline(x, y0, y1, color)` | 垂直线（含两端）|
| `ui_line(x0, y0, x1, y1, color)` | 任意直线（Bresenham，1 像素宽）|
| `ui_line_thick(x0, y0, x1, y1, thickness, color)` | **粗直线**，`thickness` 像素宽 |
| `ui_line_dashed(x0, y0, x1, y1, on_len, off_len, color)` | **虚线**，画 `on_len` 跳 `off_len` |

### 例子

```c
/* 在波形区中间画一个 120x60 的实心蓝框 */
ui_fill(180, 90, 120, 60, ui_rgb(0, 60, 180));

/* 在它外面套一个 2px 厚的白色空心框（画两圈） */
ui_rect(176, 86, 128, 68, C_WHITE);
ui_rect(177, 87, 126, 66, C_WHITE);

/* 一条对角线 */
ui_line(0, 20, 479, 226, C_GRID_MAJ);

/* 分隔线 */
ui_hline(0, 479, 150, C_EDGE);
ui_vline(240, 20, 226, C_EDGE);
```

### 直线：细线 / 粗线 / 虚线

三个都是 Bresenham 整数算法，没有浮点、没有除法，任意方向、端点都包含。

```c
/* 1 像素细线 */
ui_line(10, 20, 400, 200, C_CH1);

/* 粗线：thickness 像素宽（建议给奇数，居中最好看）
 * 内部沿路径逐点盖一个 thickness×thickness 小方块，所以斜线也不会出现断口 */
ui_line_thick(10, 40, 400, 220, 3, C_CH2);      /* 3 像素粗 */
ui_line_thick(10, 60, 400, 60,  5, C_TRIG);     /* 5 像素粗的横线 */

/* 虚线：画 on_len 个点、跳 off_len 个点，循环 */
ui_line_dashed(10, 240, 470, 240, 6, 4, C_GRID_MAJ);   /* 6 亮 4 暗 */
ui_line_dashed(10, 250, 470, 250, 2, 4, C_DIM);        /* 点线 */
ui_line_dashed(10, 260, 470, 260, 4, 0, C_WHITE);      /* off=0 就是实线 */
```

| 函数 | 局限 |
|---|---|
| `ui_line` | 1 像素宽，无抗锯齿（斜线有锯齿是正常的）|
| `ui_line_thick` | 细长斜线看起来会偏"方"，因为逐点盖方块；对一般 UI 足够 |
| `ui_line_dashed` | 只支持一条虚线段参数，做"点划线"用 `on=2, off=4` 近似 |

**注意 `ui_fill` 和 `ui_rect` 的区别**：前者把内部填满，后者只画边框。

---

## 4. 文字

字体只有两种，都是 **ASCII**（可打印字符 `0x20 ~ 0x7E`），**不支持中文**。

| | 字符尺寸 | 字间距 | 行高 | 函数 |
|---|---|---|---|---|
| Font12 | 7 × 12 | 1 px | 12 | `ui_text` / `ui_text_w` / `ui_text_c` |
| Font16 | 11 × 16 | 1 px | 16 | `ui_text16` / `ui_text_w16` / `ui_text_c16` |

```c
int  ui_text  (int x, int y, const char *s, uint16_t fg, int32_t bg);
int  ui_text16(int x, int y, const char *s, uint16_t fg, int32_t bg);
```

- `x, y`：**左上角**坐标（不是基线）
- `fg`：字的颜色
- `bg`：字的底色
  - 传具体颜色 → 字模的底色也会被填掉（背景不透明，适合画在彩色块上）
  - 传 **`UI_BG_NONE`** → 只画笔画的像素，背景透明（适合画在网格/波形上）
- 返回值：这段文字的宽度（像素，含最后的 1px 字间距），方便接着往右排

### 例子

```c
/* 最简单的：左上角白字 */
ui_text(8, 4, "HELLO", C_WHITE, UI_BG_NONE);

/* 画在深色条上，底色一起填掉 */
ui_text(8, 4, "CH1  1.00kHz", C_CH1, C_ST_BG);

/* 居中 */
ui_text_c(240, 100, "CENTERED", C_WHITE, UI_BG_NONE);

/* 横向排版：接着上一个往后写 */
int x = 8;
x += ui_text(x, 4, "RUN", C_GREEN, UI_BG_NONE);
x += 10;                                  /* 空 10 px */
x += ui_text(x, 4, "10us/div", C_WHITE, UI_BG_NONE);

/* 大字号 */
ui_text16(20, 60, "1.65 V", C_AMBER, UI_BG_NONE);
```

### 数字转字符串

**不要用 `printf` / `sprintf` 的浮点格式**（MicroLIB 下会链进几十 KB 库，而且慢）。
工程已经公开了三个定点格式化函数，全部返回**新的字符串末尾指针**，所以能链式拼接：

```c
char *ui_str (char *p, const char *s);       /* 拷贝字符串 */
char *ui_itoa(char *p, int32_t v);           /* 整数     1234 -> "1234"    -56 -> "-56" */
char *ui_fix (char *p, int32_t v, int dec);  /* 定点小数  v = 值 × 10^dec */
```

`ui_fix` 的 `dec` 是小数位数：

| 调用 | 结果 |
|---|---|
| `ui_fix(p, 165, 2)` | `"1.65"` |
| `ui_fix(p, 3300, 2)` | `"33.00"` |
| `ui_fix(p, -5, 1)` | `"-0.5"` |
| `ui_fix(p, 42, 0)` | `"42"`（dec=0 就是纯整数）|

**基本用法（链式拼接，最后画一次）**

```c
char buf[32], *p = buf;

p = ui_str(p, "CH1 = ");
p = ui_fix(p, 165, 2);
p = ui_str(p, " V");                       /* buf 现在是 "CH1 = 1.65 V" */
ui_text(8, 4, buf, C_WHITE, UI_BG_NONE);
```

**ADC 码值 → 电压（最常用）**：12 位码值 0..4095 对应 0..3.3 V

```c
uint16_t code = 2048;
int32_t  mv   = (int32_t)code * 3300 / 4095;    /* 毫伏，纯整数运算，不用浮点 */

p = ui_str(p, "CH1 ");
p = ui_fix(p, mv, 3);                           /* dec=3 -> "1.650" */
p = ui_str(p, " V");
```

**整数也用 `ui_itoa`**（自带负号，不用自己判符号）

```c
p = ui_str(p, "CNT ");
p = ui_itoa(p, count);
```

- 缓冲区留 16~32 字节足够
- 三个函数都是**先写 `*p` 结尾的 `\0` 再返回新末尾**，所以可以直接接下一次
- 只想要电压浮点值、不在乎速度，用 `ui_code_to_volt(code)`（见第 11.3 节）

---

## 5. 自定义内容画在哪里（★ 重要）

`ui_draw()` 每帧都会重画波形区，所以**从主循环里随便画会被覆盖**。正确做法是注册一个回调：

```c
void ui_set_custom(void (*fn)(void));
```

`ui_draw()` 每帧在"波形画完之后、翻页之前"调用它，你的内容永远在最上层。

```c
/* 我的自定义绘制 */
static void my_overlay(void)
{
    ui_rect(300, 30, 100, 40, C_WHITE);
    ui_text(306, 36, "MARK", C_WHITE, UI_BG_NONE);
}

int main(void)
{
    ...
    ui_init();
    ui_set_custom(my_overlay);        /* 注册一次即可 */
    while (1) { ... ui_draw(&w); }
}
```

### 画在不同区域的注意事项

| 想画在 | 会怎样 | 建议 |
|---|---|---|
| **状态栏**（y 0~19）| 会被状态栏重画覆盖（只在状态变化时）| 直接改 `ui_render.c` 的 `draw_status()` |
| **工具条**（y 227~271）| 同上 | 直接改 `draw_toolbar()` |
| **波形区**（y 20~226）| 轨迹经过的地方会被"擦轨迹"逻辑还原成网格，下一帧钩子再画回来 → 那一小片有轻微闪 | 用钩子，内容尽量小而静态；或把 `ui_render.c` 里 `UI_PARTIAL_ERASE` 改成 `0`（回到整片重画，不闪但整片会闪）|

---

## 6. 改状态栏 / 工具条

如果你要改的是那两个区域，直接改 `ui_render.c` 里的两个函数：

### `draw_status()` —— 状态栏（y 0~19）

```c
static void draw_status(const wave_window_t *w)
{
    fillrect(0, ST_Y, SCR_W, ST_H, C_ST_BG);       /* 底色 */
    fillrect(4, ST_Y + 6, 8, 8, runc);             /* 左边那个小方块 */
    txt(&Font12, 15, ST_Y + 4, "RUN", runc, C_ST_BG);
    ...
}
```

改完必须让缓存失效才看得到 —— 两个办法：

1. 改了 `g_ui` 里影响状态栏的字段（`run` / `scan` / `ch_mask` / `fps_x10` / `ndrop` / `crc_err` / `trig_*`）→ `chrome_sig()` 变了，下一帧自动重画
2. 直接调 **`ui_force_redraw()`** 强制下一帧重画

### `draw_toolbar()` —— 工具条（y 227~271）

按钮的标签表在这儿：

```c
static const char *s_tb_lab[8] = { "RUN", "SCAN", "CH1", "CH2", "D1", "D2", "D3", "MENU" };
```

改标签、改按钮数、改配色都在这个函数里（`TB_N = 8`、`TB_W = 60` 在 `ui_layout.h`）。

---

## 7. 完整可跑的例子

在状态栏右边加一个自定义图标，在波形区右上角画一个"测量游标"方框：

```c
#include "ui_render.h"
#include "ui_layout.h"

/* 每帧刷新 */
static uint16_t s_my_value = 0;          /* 想显示的数字，别处更新它 */

static void my_overlay(void)
{
    char buf[16];
    int  x, w;

    /* ---- 波形区右上角：空心方框 + 标题 ---- */
    ui_rect(360, 24, 112, 44, C_WHITE);                    /* 空心框 */
    ui_text(366, 28, "CURSOR", C_WHITE, UI_BG_NONE);

    /* ---- 里面显示一个数值（整数转字符串） ---- */
    {
        char *p = buf;
        uint16_t v = s_my_value;
        char t[8];
        int n = 0;
        do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
        while (n) *p++ = t[--n];
        *p = 0;
    }
    x = 360 + 56 - ui_text_w(buf) / 2;                     /* 水平居中 */
    ui_text(x, 46, buf, C_AMBER, UI_BG_NONE);

    /* ---- 两个游标竖线 ---- */
    ui_vline(200, 20, 226, C_D2);
    ui_vline(300, 20, 226, C_D2);

    /* ---- 一条自定义横线 ---- */
    ui_hline(200, 300, 100, C_D1);

    (void)w;
}

int main(void)
{
    /* ... 前面的初始化 ... */
    ui_init();
    ui_set_custom(my_overlay);

    while (1)
    {
        /* ... 造数据、ui_draw ... */
        s_my_value = (uint16_t)(HAL_GetTick() / 10u);      /* 随便变个数 */
    }
}
```

---

## 7.5 ★ 数据刷新（最常问的问题）

### 刷新是怎么驱动的

```
main() 主循环              每 40 ms 一次  →  25 fps
   └── ui_draw(&w)
         ├── 擦上一帧波形
         ├── 画新波形
         ├── 画读数面板
         └── 调你注册的钩子   ← 你的刷新代码写在这里，每帧都会执行
```

所以"刷新数据"= **在钩子里每帧重画那一小块区域**。

### 核心：会变的内容必须自己先擦

裸帧缓冲没有背景重绘。直接盖新值会留残影：

```
旧值 "100"  →  新值 "9"  →  屏幕上变成 "900"   ← 后两位没被擦掉
```

三种写法，按需要选：

| 写法 | 每帧开销 | 用在哪 |
|---|---|---|
| **`ui_field(id, ...)`** | **几乎为零**（内容没变就跳过）| **首选**：数值、状态文字 |
| `ui_fill(...)` + 再画 | 擦一块 + 画 | 图形类：进度条、指针、游标 |
| `ui_text_at(...)` | 擦一块 + 画 | 简单场景，内容每帧都变 |

### `ui_field()` —— 刷新数值就用它

```c
char buf[16];
put_i(buf, (int)(HAL_GetTick() / 100));

/* id 取 0~7，每块数字域用一个固定 id */
ui_field(0, 100, 40, 80, 16, buf, C_WHITE, C_BG);
```

- 返回值 `1` = 这次真重画了，`0` = 内容没变、完全跳过
- **内容不变时一个像素都不写** → 不占总线、不会闪
- 不同 `id` 各自缓存自己的上次字符串，互不干扰。界面上有 5 个数字就用 `id` 0~4

### `ui_text_at()` —— 每次都重画的简单版

```c
ui_text_at(100, 40, 80, 16, "HELLO", C_WHITE, C_BG);
```

先把 `(x,y,w,h)` 整块刷成 `bg`，再把字居中画上去。每帧调用都不会留残影，只是比 `ui_field` 费。

### 三个坑

1. **区域被整体清掉后要 `ui_field_reset()`**
   否则 `ui_field` 以为"内容没变"不画 → 那块空着。
   `ui_force_redraw()` 内部已经自动调了，所以切页 / 改 `g_ui` 时不用管。

2. **`ui_fill` 要擦整个"变化范围"，不是只擦新值那么大**
   进度条从 80% 退到 30%：
   ```c
   ui_fill(PX0 + 1, y + 14, bw - 2, 16, C_BG);                 /* 先擦满整条 */
   ui_fill(PX0 + 1, y + 14, pv * (bw - 2) / 100, 16, C_GREEN); /* 再画新的 */
   ```

3. **擦的区域颜色要和底下一致**
   擦成 `C_BG` 会在网格上留一块黑底 —— 看起来像"仪表读数框"，其实挺正常。
   想完全融入网格背景就得走内部那套 `bg_at()`，一般不值当。

### 完整例子：一个实时数值面板

```c
static void my_overlay(void)
{
    char    buf[16], *p;
    int32_t mv = (int32_t)((HAL_GetTick() * 37u) % 3300u);   /* 0..3299 mV */

    /* 静态部分：边框 + 单位 */
    ui_rect(360, 24, 112, 44, C_EDGE);
    ui_text(430, 46, "V", C_DIM, UI_BG_NONE);

    /* 组装 "1.65" 这种定点字符串 */
    p = buf;
    p = put_i(p, mv / 1000);
    *p++ = '.';
    *p++ = (char)('0' + (mv / 100) % 10);
    *p++ = (char)('0' + (mv / 10) % 10);
    *p = 0;

    /* 内容不变 -> 完全不重画 */
    ui_field(0, 366, 44, 60, 18, buf, C_AMBER, C_BG);
}
```

> `put_i` 是 `ui_demo.c` 里的 `static` 小工具（数字转字符串），可以自己复制一份，
> 或者直接用 `sprintf(buf, "%d", v)`（整数格式化 MicroLIB 支持）。

### 数据来自中断怎么办

中断里**只置标志 / 存快照**，主循环里再刷屏：

```c
volatile uint16_t g_adc_raw;          /* 中断里更新 */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)   /* ADC 中断 */
{
    g_adc_raw = HAL_ADC_GetValue(h);   /* 只存数，不碰屏幕 */
}

static void my_overlay(void)
{
    char buf[16];
    put_i(buf, (int)g_adc_raw);
    ui_field(0, 100, 40, 80, 16, buf, C_WHITE, C_BG);   /* 主循环里画 */
}
```

**千万不要在中断里调 `ui_*` 绘图函数** —— 一次 `ui_fill` 可能上万次写内存，会把所有低优先级中断和主循环全堵死。

---

## 8. 常见问题

| 现象 | 原因 |
|---|---|
| 画了但看不见 | ① 坐标越界 ② 被 `ui_draw()` 覆盖了（要放进 `ui_set_custom` 的钩子）③ 颜色和底色一样 |
| 文字显示成一堆方块 | 用了中文/非 ASCII 字符。字库只有 `0x20~0x7E` |
| 文字背景是花的 | `bg` 传了 `UI_BG_NONE`，而底下是波形；改成具体颜色 |
| 改了状态栏没反应 | 忘了调 `ui_force_redraw()`，或者改的是 `ui_layout.h` 但没重新编译 |
| 波形区的东西一闪一闪 | 擦轨迹逻辑和你的绘制打架，见 §5 的表格 |
| 编译报 `implicit declaration` | 忘了 `#include "ui_render.h"` |

---

## 9. 触摸屏

### 硬件

| 项 | 值 |
|---|---|
| 芯片 | **FT5336** 电容触摸控制器 |
| 总线 | **I2C3**，PH7 = SCL / PH8 = SDA，AF4 |
| 地址 | 7 位 `0x38` → HAL 传 **`0x70`** |
| 中断脚 | PI13（本实现用**轮询**，不用中断）|
| 分辨率 | 480 × 272，面板出厂竖屏装 → **读出来 X/Y 要交换** |

驱动在 `Core/Src/touch.c`：`Touch_Init()` / `Touch_Scan()` / `Touch_Poll()`。

### 用法（主循环里）

```c
#include "touch.h"

static Touch_State s_ts;

if (Touch_Init() != TOUCH_OK) { /* 没找到，检查 I2C3 */ }

while (1)
{
    uint8_t ev = Touch_Poll(&s_ts);        /* 轮询 + 边沿检测，25~100Hz 都行 */

    if (ev & TOUCH_EV_DOWN)                /* 刚按下 */
    {
        int w = ui_toolbar_hit((int)s_ts.x, (int)s_ts.y);   /* 点中按钮了吗 */
        if (w != UI_WID_NONE)
            ui_handle_widget(w);
    }

    /* 把位置交给界面画十字准星；松开传 -1 */
    ui_touch_set(s_ts.pressed ? (int)s_ts.x : -1,
                 s_ts.pressed ? (int)s_ts.y : -1,
                 (int)s_ts.pressed);

    /* ... ui_draw() ... */
}
```

`Touch_Poll()` 返回的事件位：

| 位 | 含义 |
|---|---|
| `TOUCH_EV_DOWN` | 刚按下（一次触摸只触发一次）|
| `TOUCH_EV_UP` | 刚抬起 |
| `TOUCH_EV_MOVE` | 按住并且位置变了 |

### 工具条按钮编号

```c
#define UI_WID_RUN   0
#define UI_WID_SCAN  1
#define UI_WID_CH1   2
#define UI_WID_CH2   3
#define UI_WID_D1    4
#define UI_WID_D2    5
#define UI_WID_D3    6
#define UI_WID_MENU  7
```

`ui_toolbar_hit(x, y)` 把屏幕坐标映射成按钮号，返回 `UI_WID_NONE` 表示没点中。
命中区域是整格 60×45 px（比按钮实际画的矩形大一点，方便手指按）。

### 自己加可点击区域

不用改界面代码，直接在应用层判断：

```c
if (ev & TOUCH_EV_DOWN)
{
    int x = (int)s_ts.x, y = (int)s_ts.y;

    if (x >= 360 && x < 472 && y >= 24 && y < 68)
    {
        /* 点中了右上角那块自定义面板 */
    }
}
```

### 触摸准星（默认**关闭**）

`ui_touch_set(x, y, pressed)` 只是把坐标交给界面。是否画那个 15×15 的红十字，由**开关**控制：

```c
void ui_cursor_enable(int on);      /* 0 = 不画（默认），1 = 画 */
int  ui_cursor_is_on(void);         /* 查当前状态 */
```

| 调用 | 效果 |
|---|---|
| 不调 | **不画十字**（默认就是关的）|
| `ui_cursor_enable(1)` | 按下时在波形区画红十字，抬起自动擦掉 |
| `ui_cursor_enable(0)` | 关掉，并把屏幕上的十字擦干净 |

```c
/* 开机时想要十字就加这一句（不加就没有）*/
ui_cursor_enable(1);
```

要点：

- **只影响画不画，完全不影响触摸功能** —— `ui_button()` 照样正常响应
- 十字只在**波形区（y 20~226）**画；工具条/状态栏上不画（那里靠按钮变色反馈）
- 擦除走 `bg_at()`，所以不会在网格上留下痕迹
- 随时可调，运行中切换也行（比如做成一个设置项）

### 为什么不用 emWin 那套校准

emWin 手册第 22 章的 `GUI_TOUCH_X_ActivateX/MeasureX` + `GUI_TOUCH_Calibrate(AD_LEFT/AD_RIGHT/...)`
是给**四线电阻屏**用的 —— 那种屏只能量出模拟电压，所以必须标定 4 个边界 AD 值。

**FT5336 是电容屏，I2C 直接给出数字坐标**，不存在校准问题，拿到 (x, y) 就能用。
能借用手册的只有它的模型：

```
emWin：每 100Hz 调 GUI_TOUCH_Exec() → 驱动调 GUI_TOUCH_StoreState(x, y)（负数 = 未按下）
本工程：每帧调 Touch_Poll()          → 得到 (x, y, pressed) → ui_toolbar_hit() 命中判断
```

---

## 10. ★★ 快速生成触摸按键 `ui_button()`

> **只想马上用**：看 `触摸屏使用说明.md` 的 **第 0 章 快速上手**（4 步最简用法）。
> 本章讲完整参数、内部行为和组合示例。

一个函数搞定「画按钮 + 判命中 + 防连发 + 状态配色」，每帧调一次即可。

> ⚠ **按钮对象必须是 static / 全局变量**，不能用普通局部变量 ——
> `ui_button()` 要在帧与帧之间记住"绘制缓存 / 视觉状态 / 手指是否按着"，
> 局部变量每帧重建会让这三样全丢 → 按钮每帧重画（会闪）、按住状态错乱。
> 也不要用 `= {0}` 初始化（配色全黑看不见），要用 `UI_BTN_DEFAULT`。
> **完整说明见 `触摸屏使用说明.md` 第 0.3 节「按键的初始化」。**

### 三步上手

```c
/* ① 定义按钮对象（必须用 UI_BTN_DEFAULT 初始化，它会填好默认配色） */
static ui_btn_t b_start = UI_BTN_DEFAULT;
static ui_btn_t b_stop  = UI_BTN_DEFAULT;
static int      running;            /* 自己的状态 */

/* ② 在 ui_set_custom() 注册的回调里，每帧调一次 */
static void my_ui(void)
{
    if (ui_button(&b_start,  20, 40, 110, 36, "START", running))
        running = 1;                /* 被点了一下 */

    if (ui_button(&b_stop,  145, 40, 110, 36, "STOP",  !running))
        running = 0;
}

/* ③ 注册 */
ui_set_custom(my_ui);
```

就这些 —— 不用自己判坐标、不用自己重画、不用管防抖。

### 函数原型

```c
int ui_button(ui_btn_t *b, int x, int y, int w, int h,
              const char *label, int on);
```

| 参数 | 说明 |
|---|---|
| `b` | 按钮对象，用 `UI_BTN_DEFAULT` 初始化一次 |
| `x, y, w, h` | 矩形（左上角 + 宽高），每帧可改（支持动态布局）|
| `label` | 按钮文字，ASCII；传 `0` 表示不写字（可以自己画图标）|
| `on` | `1` = 显示为「激活/选中」配色 |
| **返回** | **`1` = 这次触摸点中了它**（按下瞬间触发一次）|

### 它自动做了什么

| 行为 | 说明 |
|---|---|
| **画** | 底色 + 1px 边框 + 居中文字；按下或 `on` 时切到高亮配色 |
| **只在变化时重画** | 内部记 `last_vis`，外观没变就一个像素都不碰 → 完全不闪 |
| **命中判断** | 用 `ui_touch_set()` 传进来的全局触摸位置 |
| **防连发** | 按住不放只触发一次，抬手才能再触发 |
| **防连击** | 同一次触摸只会有一个按钮被触发（手指划过多个按钮不会全响）|

### 改配色

```c
ui_btn_t b = UI_BTN_DEFAULT;

/* 松开时：白字 / 深灰底；激活时：黑字 / 黄底 */
ui_btn_colors(&b, C_WHITE, C_TB_BTN, C_BLACK, C_CH1);

ui_button(&b, 10, 10, 90, 30, "OK", 0);
```

默认配色：松开 = 深蓝底白字 + 灰边；激活/按下 = 亮蓝底绿字。

配色常量都在 `ui_layout.h`：`C_WHITE` `C_BLACK` `C_RED` `C_GREEN` `C_AMBER` `C_CH1` `C_CH2`
`C_BG` `C_EDGE` `C_TB_BTN` `C_TB_BTN_ON` `C_TB_TX` `C_GRID_MAJ` `C_GRID_MIN` `C_DIM` …

### 运行时初始化：`ui_btn_init()`

```c
void ui_btn_init(ui_btn_t *b);       /* 填回默认配色，等于 b = UI_BTN_DEFAULT */
```

两种用法：

```c
/* ① 运行时批量初始化（按钮数组放在函数里 / 需要"恢复默认"时）*/
static ui_btn_t s_btn[4];
int i;

for (i = 0; i < 4; i++)
    ui_btn_init(&s_btn[i]);

/* ② 复位按钮：清掉"画过没有 / 按住中"这些状态，下一帧整块重画 */
ui_btn_init(&b1);
```

> 效果和 `= UI_BTN_DEFAULT` 完全一样，只是能写在语句里（宏不能）。
> 不能用来"清空"按钮 —— 它填的是**默认配色**，不是 `{0}`。

### 强制重画

按钮区域被别的绘制盖住之后（`ui_clear()` / `ui_reset()` / 你自己画的图），要让它知道：

```c
ui_btn_invalidate(&b1);
```

### 完整可跑例子：4 个按钮的菜单

```c
#include "ui_render.h"
#include "ui_layout.h"

#define NB  4

static ui_btn_t s_btn[NB] =
{
    UI_BTN_DEFAULT, UI_BTN_DEFAULT, UI_BTN_DEFAULT, UI_BTN_DEFAULT
};
static const char *s_lab[NB] = { "CH1", "CH2", "D1", "D2" };
static int         s_sel;                       /* 当前选中哪一个 */

static void my_menu(void)
{
    int i, x, y, w = 100, h = 34;
    int col_x = 30, row_y = 40;

    /* 标题 */
    ui_text16(30, 16, "MY MENU", C_WHITE, UI_BG_NONE);
    ui_hline(30, 450, 36, C_EDGE);

    /* 2x2 按钮阵列 */
    for (i = 0; i < NB; i++)
    {
        x = col_x + (i % 2) * (w + 16);
        y = row_y + (i / 2) * (h + 16);

        if (ui_button(&s_btn[i], x, y, w, h, s_lab[i], s_sel == i))
        {
            s_sel = i;                          /* 点中了第 i 个 */

            /* 选中态变了，让所有按钮重画一次 */
            {
                int k;
                for (k = 0; k < NB; k++) ui_btn_invalidate(&s_btn[k]);
            }
        }
    }

    /* 底部显示当前选择（内容没变就不重画）*/
    {
        char buf[16], *p = buf;
        const char *intro = "selected: ";
        while (*intro) *p++ = *intro++;
        *p++ = s_lab[s_sel][0];
        if (s_lab[s_sel][1]) *p++ = s_lab[s_sel][1];
        if (s_lab[s_sel][2]) *p++ = s_lab[s_sel][2];
        *p = 0;

        ui_field(0, 30, 150, 200, 18, buf, C_AMBER, C_BG);
    }
}

int main(void)
{
    /* ... CubeMX 初始化 ... */
    ui_init();
    ui_set_custom(my_menu);            /* 注册，之后每帧自动调 */

    while (1)
    {
        /* 触摸轮询（必须，否则 ui_button 收不到坐标）*/
        if (Touch_Poll(&ts) & TOUCH_EV_DOWN) { /* 工具条命中，可选 */ }
        ui_touch_set(ts.pressed ? ts.x : -1, ts.pressed ? ts.y : -1, ts.pressed);

        /* 25fps 刷屏 */
        ui_draw(&w);
    }
}
```

> ⚠️ **`ui_button()` 必须在 `ui_set_custom()` 的回调里调用**（也就是在 `ui_draw()` 内部），
> 因为只有那时触摸坐标已经通过 `ui_touch_set()` 更新好了、而且绘图目标是当前帧缓冲。
> 别在 `ui_draw()` 外面直接调。

### 不想要按钮，只想要"点一块区域"

直接用 `ui_touch_set()` 存的坐标自己判断就行：

```c
static void my_ui(void)
{
    /* 自己画一个方块 */
    ui_rect(300, 30, 100, 40, C_WHITE);
    ui_text(310, 40, "TAP ME", C_WHITE, UI_BG_NONE);

    /* 自己判命中 —— 用 ui_toolbar_hit 的坐标没法直接拿，
       所以更简单的办法是用 ui_button（它把绘制和命中合成了一步）*/
}
```

结论：**要能点的区域，直接用 `ui_button()` 就行**，不用分开写绘制和命中。

---

## 11. 其余函数（框架开关 / 清屏 / 换算 / 测量）

### 11.1 `ui_chrome_enable()` —— 关掉框架，整屏自己画

```c
void ui_chrome_enable(int on);       /* 0 = 关，1 = 恢复（默认就是 1）*/
```

开着（默认）时，`ui_draw()` 每帧会画：状态栏（y 0~19）、网格、触发线、读数面板、波形、工具条（y 227~271）。

关掉之后这些**全部不再画**，并顺手清一次屏，整块 480×272 交给你的 `ui_set_custom()` 回调。

```c
int main(void)
{
    ...
    ui_init();
    ui_chrome_enable(0);          /* ★ 放在 ui_init 之后、ui_set_custom 之前 */
    ui_set_custom(my_ui);         /* 现在整屏都是我画的 */
    while (1) { ... ui_draw(&s_w); ... }
}
```

关掉之后必须自己做的两件事：

1. **静态背景要自己铺一次**（只铺一次，别每帧铺）：
   ```c
   static int inited;
   static void my_ui(void)
   {
       if (!inited) { inited = 1; ui_fill(0, 0, 480, 272, C_BG); }
       ...
   }
   ```
2. **会变的内容用 `ui_field()` 刷**（它自己擦旧值，不会留残影）

想恢复框架：`ui_chrome_enable(1);` —— 会清屏，下一次 `ui_draw()` 重画框架。

> 起点工程 `quiz` 的 `main.c` 默认就是 `ui_chrome_enable(0)`，所以上电是黑屏，
> 一切内容都要在 `ui_demo.c` 的 `ui_main()` 里自己画。想直接用现成框架就把它改成 1。

### 11.2 `ui_clear()` / `ui_reset()` —— 清屏

```c
void ui_clear(uint16_t color);   /* 整屏刷成一个颜色 */
void ui_reset(void);             /* 整屏刷黑 + 立刻重建网格/读数面板/工具条 */
```

它们和 `ui_fill(0, 0, 480, 272, color)` 的区别：**会顺手把内部缓存一起清掉**
（上一帧轨迹记录 + `ui_field` 缓存）。所以清屏**必须用 `ui_clear`**，直接用 `ui_fill` 会出两个问题：

- 下一帧拿上一帧的轨迹去"擦"，会在干净的屏上画回一堆网格碎片
- `ui_field` 会以为"内容没变"而不重画，数值永远不更新

```c
/* 想把屏幕擦干净重来 */
ui_clear(C_BG);

/* 想回到"刚上电、有网格有工具条"的样子 */
ui_reset();
```

- 清完屏之后，之前画的按钮要 `ui_btn_invalidate(&b)`，否则它以为"画过了"不会重画
- 状态栏要等下一次 `ui_draw()` 才补上（它需要 `wave_window_t` 数据，`ui_reset()` 拿不到）
- 还没 `ui_init()` 就调：`ui_clear` 直接返回，`ui_reset` 会先帮你 `ui_init()`

### 11.2.5 ★ 改整个屏幕的背景色 `ui_set_bg_color()`

**想换整屏底色（比如深蓝、白色、某个主题色），用这个：**

```c
void     ui_set_bg_color(uint16_t color);   /* 设定背景色 */
uint16_t ui_get_bg_color(void);             /* 查当前背景色 */
```

默认背景是 `C_BG`（纯黑）。调一次之后，**整屏所有底色都跟着变**：

| 受影响的地方 | 说明 |
|---|---|
| 主界面底色 | `ui_init` / `ui_reset` / `ui_clear` 全用新色 |
| 波形区底色 | `draw_grid()` 铺底用新色 |
| 读数面板底色 | 面板填充 + 文字底色用新色 |
| 擦轨迹的颜色 | `erase_traces()` 用新色还原，不会留黑边 |
| `bg_at()` 查表 | 自动重建，所以消残影仍然正确 |

```c
/* 开机调一次（在 ui_init 之后）*/
ui_init();

ui_set_bg_color(ui_rgb(0, 32, 64));       /* 深蓝 */
/* 或者 */
ui_set_bg_color(C_BLACK);                 /* 纯黑（默认）*/
ui_set_bg_color(ui_rgb(240, 240, 240));   /* 浅灰 */
```

**⚠ 调用后会立刻整屏重刷**（内部走 `ui_reset()`），已经画的内容会被清掉，
需要在 `ui_main()` 里重画。所以建议在注册界面之前调：

```c
ui_init();
ui_set_bg_color(ui_rgb(0, 32, 64));    /* ← 先设底色 */
ui_chrome_enable(0);
ui_demo_init();                        /* ← 再注册界面 */
```

**和 `ui_clear(color)` 的区别（很重要）：**

| | `ui_clear(color)` | `ui_set_bg_color(color)` |
|---|---|---|
| 作用 | **只刷一次**像素 | 改**设定**，之后所有重绘都用它 |
| 框架下次重绘 | 仍用旧背景色 | 用新底色 |
| 网格/面板 | 会被旧底色覆盖回去 | 一起变 |
| 擦轨迹 | 用旧色还原 → 留黑边 | 用新色还原 |
| 适用 | 临时清屏、局部擦除 | **换整屏主题色** |

**典型误用**：

```c
/* ✗ 错：这样只能白一帧，下一帧网格和面板又刷回黑底 */
ui_clear(ui_rgb(0, 32, 64));

/* ✓ 对：整屏主题色要改设定 */
ui_set_bg_color(ui_rgb(0, 32, 64));
```

> ⚠ 背景色**不要设成和网格线/文字相近的颜色**，否则看不清。
> `C_GRID`（#1C1C1C）和 `C_EDGE`（#4A4A4A）是深灰线，浅底上会显不出来。
> 换浅底时建议同时把网格换成深色（要改 `ui_layout.h` 里的 `C_GRID` / `C_GRID_MAJ`）。

### 11.3 时基与码值换算

```c
float       ui_code_to_volt(uint16_t code);   /* 12 位码值 -> 电压，3.3 * code / 4095 */
const char *ui_scan_label (uint8_t scan);     /* 0..4 -> "1us" "10us" "100us" "1ms" "10ms" */
uint32_t    ui_scan_div_ns(uint8_t scan);     /* 0..4 -> 1000 10000 100000 1000000 10000000 (ns) */
```

显示端要写"当前时基"或"某点的电压"，直接调这三个，不要自己再写一份：

```c
char buf[32], *p = buf;
uint16_t code = 2048;

p = ui_str(p, ui_scan_label(g_ui.scan));                      /* "10us" */
p = ui_str(p, "  ");
p = ui_fix(p, (int32_t)(ui_code_to_volt(code) * 1000.0f + 0.5f), 3);   /* "1.650" */
p = ui_str(p, " V");
ui_text(8, 4, buf, C_WHITE, UI_BG_NONE);
```

- `ui_code_to_volt` 是浮点（内部一次除法），只在"最终要显示"的地方用一次；
  纯整数要快就用 `code * 3300 / 4095` 得毫伏
- `ui_scan_div_ns` 常用来算"第 i 个点对应多少 ns"：`i * ui_scan_div_ns(g_ui.scan) / NPT`
- `scan` 传超出 0..4 会被自动夹到 4，不会越界

### 11.4 `ui_measure()` —— 算测量值

```c
void ui_measure(const wave_window_t *w, ui_meas_all_t *m);
```

输入一窗数据，输出 4 路测量结果：`m->ch1 / ch2 / d1 / d3`，
每路都是 `{ float freq, vpp, vavg, width, duty; uint8_t valid; }`。

```c
static wave_window_t s_w;
static ui_meas_all_t s_m;
char buf[32], *p = buf;

ui_measure(&s_w, &s_m);               /* 每帧算一次，500 点够快 */

if (s_m.ch1.valid)
{
    p = buf;
    p = ui_str(p, "F = ");
    p = ui_fix(p, (int32_t)(s_m.ch1.freq + 0.5f), 0);
    p = ui_str(p, " Hz   Vpp = ");
    p = ui_fix(p, (int32_t)(s_m.ch1.vpp * 1000.0f + 0.5f), 0);
    p = ui_str(p, " mV");
    ui_text(8, 4, buf, C_CH1, UI_BG_NONE);
}
```

- 内部已经 `memset`，不用自己清 `m`
- `w->n < 2` 时直接返回，全部 `valid = 0`
- 框架自带的读数面板就是用它算的（`g_ui.show_meas` 控制显示），所以它和面板数值一定一致
- `width` / `duty` 只有数字通道（D1/D3）才有意义

### 11.5 `ui_use_sdram()` —— 本板**不要调**

```c
void ui_use_sdram(void);
```

原意是切到 SDRAM 双缓冲（FB0/FB1 翻页），切过去之后波形区就不闪了。
**但本板的 SDRAM 初始化没通**（详见 `如何使用这个工程.md` 第 9 节），
一旦访问 `0xC0000000` 就死机，所以这个函数**留着但不要调用**，单缓冲也能用。

---

## 附：API 速查

```c
/* 颜色 */
uint16_t ui_rgb(r, g, b);
/* 数字 -> 字符串（返回值 = 新末尾，可链式拼接） */
char *ui_str (p, s);                        /* 拷贝字符串 */
char *ui_itoa(p, v);                        /* 整数   1234 -> "1234" */
char *ui_fix (p, v, dec);                   /* 定点小数  ui_fix(p,165,2) -> "1.65" */
/* 图元 */
void ui_px     (x, y, color);
void ui_fill   (x, y, w, h, color);          /* 实心矩形 */
void ui_rect   (x, y, w, h, color);          /* 空心矩形 */
void ui_hline  (x0, x1, y, color);
void ui_vline  (x, y0, y1, color);
void ui_line   (x0, y0, x1, y1, color);              /* 1 像素直线 */
void ui_line_thick (x0, y0, x1, y1, thickness, color);   /* 粗线 */
void ui_line_dashed(x0, y0, x1, y1, on_len, off_len, color); /* 虚线 */
/* 文字 */
int  ui_text   (x, y, s, fg, bg);            /* Font12 7x12，返回宽度 */
int  ui_text16 (x, y, s, fg, bg);            /* Font16 11x16 */
int  ui_text_w (s);                          /* 量 Font12 宽度 */
int  ui_text_w16(s);
void ui_text_c (cx, y, s, fg, bg);           /* Font12 居中 */
void ui_text_c16(cx, y, s, fg, bg);
/* 框架 */
void ui_init(void);
void ui_draw(const wave_window_t *w);
void ui_set_custom(void (*fn)(void));        /* 注册每帧回调 */
void ui_force_redraw(void);                  /* 强制重画状态栏/工具条/网格 */
void ui_chrome_enable(int on);               /* 0 = 关掉框架整屏自己画，1 = 恢复 */
void ui_use_sdram(void);                     /* ★ 本板 SDRAM 不通，不要调 */
/* 清屏（必须用这两个，不要用 ui_fill 清屏） */
void ui_clear(color);                        /* 整屏刷一个颜色，同时清轨迹/数值缓存 */
void ui_reset(void);                         /* 清屏 + 重建网格/读数面板/工具条 */
/* 背景色（整屏主题色）*/
void     ui_set_bg_color(color);             /* ★ 改整屏底色，之后所有重绘都用它 */
uint16_t ui_get_bg_color(void);              /* 查当前底色 */
/* 换算 / 测量 */
float       ui_code_to_volt(code);           /* 12 位码值 -> 电压 3.3*code/4095 */
const char *ui_scan_label(scan);             /* 0..4 -> "1us".."10ms" */
uint32_t    ui_scan_div_ns(scan);            /* 0..4 -> 1000..10000000 ns */
void        ui_measure(&w, &m);              /* 一窗数据 -> freq/vpp/vavg/width/duty */
/* 数据刷新 */
int     ui_text_at(x, y, w, h, s, fg, bg);           /* 擦一块再居中画字 */
uint8_t ui_field(id, x, y, w, h, s, fg, bg);         /* ★ 内容没变就跳过 */
void    ui_field_reset(void);                        /* 清缓存（ui_force_redraw 已含） */
/* 触摸 */
int     ui_toolbar_hit(x, y);                        /* 屏幕坐标 -> 按钮号，-1 未命中 */
void    ui_touch_set(x, y, pressed);                 /* 把触摸位置交给界面 */
void    ui_cursor_enable(on);                        /* 触摸十字准星 0=不画(默认) 1=画 */
int     ui_cursor_is_on(void);                       /* 查准星状态 */
/* ★ 快速触摸按键 */
ui_btn_t  b = UI_BTN_DEFAULT;                        /* ① 初始化（必须，且必须 static）*/
int       ui_button(&b, x, y, w, h, label, on);      /* ② 每帧调，返回 1 = 被点 */
void      ui_btn_init(&b);                           /* 运行时初始化 / 复位 */
void      ui_btn_colors(&b, fg, bg, fg_on, bg_on);   /* 改配色 */
void      ui_btn_invalidate(&b);                     /* 强制重画 */
/* 触摸驱动（touch.h） */
uint8_t     Touch_Init(void);
uint8_t     Touch_Scan(Touch_State *ts);
uint8_t     Touch_Poll(Touch_State *ts);             /* ★ 轮询+边沿，返回 TOUCH_EV_* */
```
