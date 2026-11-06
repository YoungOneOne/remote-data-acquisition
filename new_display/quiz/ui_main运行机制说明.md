# `ui_main()` 是怎么运行的

> 你写的界面函数 `ui_main()` 自己不会跑 —— 它被框架**每帧调用一次**。
> 本文讲清楚：**谁调它、什么时候调、一帧里它排第几、为什么必须是这个形式。**

---

## 1. 一句话答案

```
main() 的 while(1)
   └── 每 40ms 调一次 ui_draw(&w)
         └── ui_draw() 内部第 5 步：调 s_custom()
               └── s_custom 指向 ui_main
                     └── 你的界面代码被执行
```

`ui_main()` 是**回调函数**（callback）—— 你把它的地址交给框架，框架到点就调。

---

## 2. 完整调用链（逐层看）

### 第 ① 层：`main.c` 主循环

```c
/* main.c 第 119 行附近 */
while (1)
{
    ...
    if (HAL_GetTick() - last_ui >= 40)        /* ★ 40ms 一次 = 25fps */
    {
        last_ui = HAL_GetTick();
        frame++;
        ui_demo_fill(&w, frame);              /* 造波形数据 */
        ui_draw(&w);                          /* ★ 画一帧 */
    }
}
```

`ui_main()` 就在 `ui_draw(&w)` 里被调用。

### 第 ② 层：`ui_draw()` 内部的 8 个步骤

```c
/* ui_render.c 第 1065 行 */
void ui_draw(const wave_window_t *w)
{
    if (s_fb == 0) ui_init();                  /* 0a. 没初始化就先初始化 */
    if (w == 0 || w->n < 2) return;            /* 0b. ★ 数据不合法直接返回！*/

    touch_erase_old();                          /* 1. 擦掉上一帧触摸准星 */

    if (s_prev_valid)                           /* 2. 擦掉上一帧波形轨迹 */
        erase_traces(&s_prev, s_prev_mask);

    if (s_chrome_on)                            /* 3. 状态栏/工具条/网格 */
    {                                           /*    （内容变了才重画）*/
        ...
    }

    if (g_ui.ch_mask & CHM_CH1) trace_ana(...); /* 4. 画本帧波形 */
    if (g_ui.ch_mask & CHM_D1)  trace_dig(...);
    ...

    if (g_ui.show_meas)                         /* 5. 读数面板 */
    {
        ui_measure(w, &m);
        draw_meas(&m);
    }

    if (s_custom != 0)                          /* ★ 6. 调你的 ui_main() */
        s_custom();

    touch_draw_new();                           /* 7. 触摸准星（默认关，不画）*/

    memcpy(&s_prev, w, sizeof(s_prev));         /* 8. 存本帧，供下次擦 */
    s_prev_valid = 1;

    flip();                                     /* 9. 换页（单缓冲时是空操作）*/
}
```

**你的 `ui_main()` 在第 6 步**，位置含义：

| 前面画了 | 你的代码之后画了 |
|---|---|
| 波形轨迹 | 触摸准星 |
| 读数面板 | —— |

所以**你的内容永远盖在波形和读数面板上面**。

> 触摸准星（十字）在第 7 步，画在你上面 —— 但它**默认是关的**
> （`ui_cursor_enable(1)` 才打开），所以正常情况不会挡住你的内容。

### 第 ③ 层：`s_custom` 是什么

```c
/* ui_render.c 第 70 行 */
static void (*s_custom)(void);      /* 一个函数指针，初始是 0（空）*/
```

它就是"存的你的函数地址"。

```c
/* ui_render.c 第 1360 行 */
void ui_set_custom(void (*fn)(void))
{
    s_custom = fn;                  /* 把地址存起来 */
}
```

`ui_draw()` 里那句 `if (s_custom != 0) s_custom();` 就是**调用存下来的地址**——
C 语言里 `s_custom()` 就是"跳过去执行那个函数"。

---

## 3. 注册发生在什么时候

```c
/* main.c 第 87 行，USER CODE BEGIN 2 里 */
ui_demo_init();                     /* ← 只在这一处注册 */
```

```c
/* ui_demo.c 第 140 行 */
void ui_demo_init(void)
{
    ui_set_custom(ui_main);         /* ★ 把 ui_main 的地址交给框架 */
}
```

注意 `ui_main` **没有括号**：`ui_set_custom(ui_main)` 传的是"地址"，`ui_set_custom(ui_main())` 传的是"返回值"（而且类型也不对，编译会报错）。

**只注册一次**，开机时。之后每帧自动被调，不用再管。

### 为什么 `ui_main` 是 `static`

```c
/* ui_demo.c 第 26 行 */
static void ui_main(void)
```

`static` = **只在本文件可见**。但这不影响注册 —— 因为注册动作 `ui_demo_init()` 也在同一个文件里，
它看得见 `ui_main`，把地址传出去后，`ui_render.c` 就能通过指针调用，不需要知道名字。

这是 C 语言里"隐藏实现、只暴露接口"的常见做法。

---

## 4. 一帧的时间线

以 25fps（40ms 一帧）为例：

```
t=0ms    主循环判断 40ms 到了
         └── ui_draw(&w) 开始
               ├── 擦准星           ~0.1 ms
               ├── 擦上帧轨迹        ~1-2 ms   （500 点 × 2 通道）
               ├── 状态栏/工具条      0 ms      （缓存命中，跳过）
               ├── 画本帧波形        ~2-3 ms
               ├── 读数面板          0 ms      （内容没变，跳过）
               ├── ★ 调 ui_main()   ← 你的代码在这里跑
               ├── 画准星            ~0.05 ms
               └── flip()            0 ms
t=40ms   下一帧
```

所以你写的代码**在一个 40ms 的预算里**执行。只要你的 `ui_main()` 在 10ms 内跑完，
就完全看不出掉帧。

> **经验值**：`ui_main()` 里画几十个图元、十几个 `ui_field`，都在 1ms 以内。
> 只有画上百条线、或者用 `printf` 打大量日志才会拖慢。

---

## 5. 每帧都要重新执行的那件事

**`ui_main()` 不是"画一次就记住"，而是"每帧从头执行一遍"。**

```c
static void ui_main(void)
{
    ui_text(20, 40, "HELLO", C_WHITE, UI_BG_NONE);    /* 每帧都执行 */
}
```

这一行在第 1 帧、第 2 帧、第 1000 帧都会执行。因为画的是**同样的像素**，
所以你看不出它在重画。

这带来一个反直觉的结论：

> **静态内容也不需要特殊处理，每帧重画就行。**
> 只有"会变的内容"才需要特殊照顾（用 `ui_field`，或者自己记旧值擦除）。

### 为什么还能不闪

因为**画同样的像素**在物理上不产生任何可见变化。像素从白变成白，屏幕上看起来一动不动。

闪烁只在"先擦成黑、再画成白"这种**中间态**存在时才出现。所以：

| 写法 | 会不会闪 |
|---|---|
| 每帧重画静态文字 | ❌ 不闪（同样的像素） |
| 每帧先清屏再全画 | ✅ 会闪（中间有一段全黑） |
| `ui_field` 变了才重画 | ❌ 不闪（跳过） |
| 先擦线再画线 | 轻微（擦和画之间有几十微秒的空档） |

---

## 6. `static` 变量在 `ui_main` 里的作用

这是关键技巧。`ui_main()` 每帧被调用，但函数内的 `static` 变量**只初始化一次，值跨帧保留**：

```c
static void ui_main(void)
{
    static int frame_cnt;          /* 第 1 帧创建并清 0，之后一直累加 */
    static int y_old = -1;         /* 记住上一帧的线位置 */

    frame_cnt++;                   /* 每帧 +1，不会重置 */
    ...
}
```

对比：

```c
static void ui_main(void)
{
    int frame_cnt;                 /* ✗ 每帧都是随机值（栈上残留）*/
    int y_old = -1;                /* ✗ 每帧都重置成 -1，永远擦不掉旧线 */
}
```

**记住三个必须用 `static` 的东西**：

| 变量 | 为什么必须 static |
|---|---|
| `y_old` 等"上次的值" | 否则每帧重置，擦除逻辑失效 |
| `ui_btn_t` 按钮对象 | 否则按钮每帧重画（会闪）、按住状态错乱 |
| `inited` 标志 | 否则"只画一次"的静态背景每帧都画 |

---

## 7. 什么时候 `ui_main` 不会被调用

不是每次都调。**以下情况它一次都不会跑**：

| 情况 | 原因 | 位置 |
|---|---|---|
| `w->n < 2` | `ui_draw()` 直接 `return` | `ui_render.c:1070` |
| `w == 0` | 同上 | `ui_render.c:1070` |
| `s_custom == 0` | 没注册（没调 `ui_demo_init()`） | `ui_render.c:1129` |
| 主循环没到 40ms | `HAL_GetTick()` 判断没过 | `main.c:119` |

**最坑的是第一条**：如果 `ui_demo_fill()` 忘了设 `w->n = NPT`，`w.n` 是 0，
`ui_draw()` 直接返回，**屏幕完全没反应，而且不报错**。

```c
/* ui_demo.c 里 ui_demo_fill() 的第一句必须是这个 */
w->n = NPT;
```

---

## 8. 完整示例：从注册到执行

```c
/* ================= ui_demo.c ================= */
#include "main.h"
#include "ui_render.h"
#include "ui_layout.h"
#include "ui_demo.h"

static void ui_main(void)                    /* ① 定义（static 只在本文件可见）*/
{
    static ui_btn_t b = UI_BTN_DEFAULT;      /* ② static 状态跨帧保留 */
    static int      page;

    if (ui_button(&b, 20, 40, 110, 36, "NEXT", 0))
        page = (page + 1) % 2;               /* ③ 点击改状态 */

    ui_text16(20, 100, page ? "PAGE B" : "PAGE A", C_WHITE, UI_BG_NONE);
}

void ui_demo_init(void)                      /* ④ 注册函数 */
{
    ui_set_custom(ui_main);                  /* ★ 传地址，不加括号 */
}

void ui_demo_fill(wave_window_t *w, uint32_t frame)
{
    if (w == 0) return;
    w->n = NPT;                              /* ★ 必须设，否则 ui_draw 直接返回 */
    ...
}

/* ================= main.c ================= */
int main(void)
{
    ...
    ui_init();                               /* ⑤ 屏幕初始化 */
    ui_chrome_enable(0);
    ui_demo_init();                          /* ⑥ 注册 ui_main（只这一次）*/
    ...
    while (1)
    {
        ...
        if (HAL_GetTick() - last_ui >= 40)   /* ⑦ 每 40ms */
        {
            last_ui = HAL_GetTick();
            ui_demo_fill(&w, ++frame);
            ui_draw(&w);                     /* ⑧ 里面会调 ui_main() */
        }
    }
}
```

---

## 9. 常见疑问

**Q：`ui_main` 会被中断打断吗？**
会。它不是临界区。如果 `ui_main` 里和中断共享变量，那个变量要加 `volatile`。

**Q：能在 `ui_main` 里调 `HAL_Delay` 吗？**
不行。会把整帧拖慢，屏幕明显卡顿。要延时用"记时间戳 + 下次比较"的方式：

```c
static uint32_t last;
if (HAL_GetTick() - last > 500) { last = HAL_GetTick(); /* 每 500ms 干一次 */ }
```

**Q：能在 `ui_main` 里改 `ui_main` 吗（比如切页面用不同函数）？**
能。回调随时可以换：

```c
ui_set_custom(page == 0 ? ui_page_a : ui_page_b);
```
下一帧就换成新的了。

**Q：`ui_main` 一定要叫这个名字吗？**
不用。名字随便，只要通过 `ui_set_custom()` 注册就行。`ui_main` 只是本工程的约定。

**Q：能注册多个界面函数吗？**
不能，`s_custom` 只有一个槽。要多个画面就写在一个函数里按状态分支，或者用函数指针自己转发。

**Q：怎么知道 `ui_main` 在跑？**
在开头加一句 `printf("tick\n");`，用 Keil 的 `View → Serial Windows → Debug (printf) Viewer` 看。
有输出就是在跑（注意 `printf` 走 SWO，不是串口）。

---

## 10. 一句话总结

```c
ui_set_custom(ui_main);      /* 开机：把地址交给框架 */

/* 之后每 40ms，ui_draw() 内部调用 s_custom() -> 你的 ui_main() */
```

**三句话**：

1. **`ui_main()` 是被"函数指针"回调的，不是被直接调用的**
2. **每帧从头执行一遍，画的是同样的像素所以看不出重画**
3. **跨帧要记住的东西必须用 `static`**
