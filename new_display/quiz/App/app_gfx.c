/**
  ******************************************************************************
  * @file    app_gfx.c
  * @brief   显示端界面渲染（直接写 LTDC 帧缓冲，不用 STemWin）
  *
  *          帧缓冲：优先 SDRAM 三缓冲，失败时退回 SRAM 0x20010000 单缓冲
  *          字体  ：ST 经典 EVAL 字库 Font12(7x12) / Font16(11x16)
  *                  格式 = 每行 (Width+7)/8 字节、行优先、字节内 MSB 在左
  *
  *          布局常量全部来自 app_layout.h，改布局只改那个文件。
  ******************************************************************************
  */
#include "app_gfx.h"
#include "app_layout.h"
#include "app_screen.h"
#include "app_ui.h"
#include "drv_lcd.h"
#include "drv_font.h"
#include <string.h>
#include <stdio.h>

/* 字库尺寸（与 font12.c / font16.c 里的 sFONT 对应） */
#define F12H            12

ui_state_t g_ui;

/* SDRAM 可用时轮换三块显存；否则使用 SRAM 单缓冲。 */
static uint16_t *s_fb;             // 当前绘制目标
static uint16_t *s_show;           // 开机时交给 LCD 的显存
static uint16_t *s_fbs[3];
static uint8_t s_fb_idx;
static uint8_t s_triple;

/* LTDC 不能读 DTCM，因此从 AXI SRAM 的 0x20010000 开始存放。 */
#define FB_SRAM ((uint16_t *)0x20010000u)

static wave_window_t s_prev;       // 单缓冲需要先擦掉上一帧轨迹
static uint8_t s_prev_mask;
static uint8_t s_prev_valid;
static ui_state_t s_last_ui;
static uint8_t s_redraw = 1;       // 切页或配置改变时，重画背景
static uint8_t s_page = UI_PAGE_WAVE;

static int s_touch_x = -1, s_touch_y = -1, s_touch_down;
static uint8_t s_touch_consumed;   // 一次按下只响应一个按钮
static uint32_t s_draw_gen;        // 换了绘制缓冲后，按钮也要重画

static void flip(void)
{
    if (s_triple) drv_lcd_set_fb_nowait(s_fb);
    drv_lcd_wait_vsync();
    if (s_triple) {
        s_fb_idx = (s_fb_idx + 1) % 3;
        s_fb = s_fbs[s_fb_idx];
        s_draw_gen++;
    }
}

static uint16_t bg_at(int x, int y);    /* 背景色查询，定义在后面 */

/* 1 = 本次绘制是"擦除上一帧"，像素值取背景而不是传进来的颜色 */
static uint8_t s_restore;

static void px(int x, int y, uint16_t c)
{

    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
        s_fb[(uint32_t)y * SCR_W + (uint32_t)x] = s_restore ? bg_at(x, y) : c;
}

static void fillrect(int x, int y, int w, int h, uint16_t c)
{
    int i, j, x1, y1;

    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    x1 = x + w; if (x1 > (int)SCR_W) x1 = (int)SCR_W;
    y1 = y + h; if (y1 > (int)SCR_H) y1 = (int)SCR_H;
    if (x1 <= x || y1 <= y) return;

    for (j = y; j < y1; j++)
    {
        uint16_t *row = &s_fb[(uint32_t)j * SCR_W];
        for (i = x; i < x1; i++)
            row[i] = c;
    }
}

static void hline_(int x0, int x1, int y, uint16_t c)
{
    int i, t;
    uint16_t *row;

    if (y < 0 || y >= (int)SCR_H) return;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 > (int)SCR_W - 1) x1 = (int)SCR_W - 1;
    if (x1 < x0) return;

    row = &s_fb[(uint32_t)y * SCR_W];
    for (i = x0; i <= x1; i++)
    {

            row[i] = s_restore ? bg_at(i, y) : c;
    }
}

static void vline_(int x, int y0, int y1, uint16_t c)
{
    int i, t;

    if (x < 0 || x >= (int)SCR_W) return;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 > (int)SCR_H - 1) y1 = (int)SCR_H - 1;
    if (y1 < y0) return;

    for (i = y0; i <= y1; i++)
    {

            s_fb[(uint32_t)i * SCR_W + (uint32_t)x] = s_restore ? bg_at(x, i) : c;
    }
}

static void rect_(int x, int y, int w, int h, uint16_t c)
{
    hline_(x, x + w - 1, y, c);
    hline_(x, x + w - 1, y + h - 1, c);
    vline_(x, y, y + h - 1, c);
    vline_(x + w - 1, y, y + h - 1, c);
}

/* ==========================================================================
 *  2. 字库渲染
 *     bg < 0 表示背景透明（只画"亮"的点）
 * ========================================================================== */

static void glyph(const sFONT *f, int x, int y, char ch, uint16_t fg, int32_t bg)
{
    int r, c, bpr;
    const uint8_t *p;

    if (ch < ' ' || ch > '~') ch = ' ';
    bpr = ((int)f->Width + 7) / 8;
    p = f->table + (uint32_t)(ch - ' ') * (uint32_t)f->Height * (uint32_t)bpr;

    for (r = 0; r < (int)f->Height; r++)
    {
        int yy = y + r;
        if (yy < 0 || yy >= (int)SCR_H) continue;

        /* 循环到 Width（含）为止：最后那一列是字间距。
         * bg >= 0 时必须把它也填掉，否则放在彩色块上的文字每两个字符之间
         * 会露出 1px 的底色缝。 */
        for (c = 0; c <= (int)f->Width; c++)
        {
            int xx = x + c;
            int ink;

            if (xx < 0 || xx >= (int)SCR_W) continue;

            if (c == (int)f->Width)
            {
                ink = 0;                        /* 字间距永远不画笔画 */
            }
            else
            {
                uint8_t b = p[r * bpr + (c >> 3)];
                ink = (b & (0x80 >> (c & 7))) ? 1 : 0;
            }

            if (ink)
                s_fb[(uint32_t)yy * SCR_W + (uint32_t)xx] = fg;
            else if (bg >= 0)
                s_fb[(uint32_t)yy * SCR_W + (uint32_t)xx] = (uint16_t)bg;
        }
    }
}

static int tw(const sFONT *f, const char *s)
{
    return (int)strlen(s) * ((int)f->Width + 1);
}

static void txt(const sFONT *f, int x, int y, const char *s, uint16_t fg, int32_t bg)
{
    while (*s)
    {
        glyph(f, x, y, *s, fg, bg);
        x += (int)f->Width + 1;
        s++;
    }
}

static void txt_c(const sFONT *f, int cx, int y, const char *s, uint16_t fg, int32_t bg)
{
    txt(f, cx - tw(f, s) / 2, y, s, fg, bg);
}

/* ==========================================================================
 *  3. 数字格式化（不依赖 printf / libc 浮点格式化）
 * ========================================================================== */

static char *put_u(char *p, uint32_t v)
{
    char t[11];
    int n = 0;

    do { t[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v && n < 11);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

/* v 是"值 × 10^dec"，输出定点小数 */
static char *put_fix(char *p, int32_t v, int dec)
{
    uint32_t div = 1;
    int32_t ip, fp;
    int i;

    for (i = 0; i < dec; i++) div *= 10u;
    if (v < 0) { *p++ = '-'; v = -v; }

    ip = v / (int32_t)div;
    fp = v % (int32_t)div;
    p = put_u(p, (uint32_t)ip);

    if (dec > 0)
    {
        *p++ = '.';
        for (i = (int)(div / 10u); i > 0; i /= 10)
            *p++ = (char)('0' + (int)(fp / i) % 10);
        *p = 0;
    }
    return p;
}

static char *put_s(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}

/* ==========================================================================
 *  4. 时基 / 换算
 * ========================================================================== */

static const uint32_t s_div_ns[5]  = SCOPE_DIV_NS;
static const char    *s_div_lab[5] = SCOPE_DIV_LABELS;

uint32_t ui_scan_div_ns(uint8_t scan)
{
    return s_div_ns[scan > 4u ? 4u : scan];
}

const char *ui_scan_label(uint8_t scan)
{
    return s_div_lab[scan > 4u ? 4u : scan];
}

float ui_code_to_volt(uint16_t code)
{
    return (float)code * 3.3f / 4095.0f;
}

/* 模拟值只映射到上方模拟区，数字轨迹使用独立的三条车道。 */
static int ana_grid_y(int i)
{
    return (int)ANA_Y + i * ((int)ANA_H - 1) / (int)ANA_DIVS;
}

static int ana_y(uint16_t code)
{
    if (code > 4095u) code = 4095u;
    return (int)ANA_Y + (int)ANA_H - 1 - (int)((uint32_t)code * (ANA_H - 1u) / 4095u);
}

/* 横向十格与采样间隔对应，不随通道开关改变。 */
static int x_of(int i, const wave_window_t *w)
{
    if (w->n <= 1) return 0;
    // 协议固定每格 50 点，五档时基都用相同的屏幕坐标。
    return i * (int)DW / (int)PPD;
}

/* ==========================================================================
 *  背景色查询：给定 (x,y) 返回"没有波形时这里应该是什么颜色"
 *
 *  优先级必须严格照抄 draw_grid() / draw_trig() 的画法（后画的覆盖先画的）：
 *      背景 < 模拟区横格线 < 竖格线 < 数字区 lane 分隔线 < 触发线
 *  擦除上一帧轨迹时就是靠它把每个像素还原成正确的底色。
 * ========================================================================== */
/* ==========================================================================
 *  背景色查表
 *
 *  按行预存横格线颜色、按列预存竖格线颜色，bg_at() 只需查表加判断。
 *  擦一帧轨迹要调用上万次，线性搜索会明显拖慢刷新。
 * ========================================================================== */
static uint16_t s_row_color[SCR_H];     /* 每行底色：背景 / 横格线 / lane 分隔线 */
static uint16_t s_vcol_color[SCR_W];    /* 每列竖线颜色，0 = 该列没有竖线
                                         * （C_BG=0x0000，而格线颜色都非 0，所以 0 能安全当"无"标记） */
static int      s_trig_y;               /* 触发电平线所在行，缓存免掉每像素一次 ana_y() */

static void bg_tables_init(void)
{
    int i, y, x;

    for (y = 0; y < (int)SCR_H; y++)
        s_row_color[y] = C_BG;

    for (i = 0; i <= (int)ANA_DIVS; i++) {
        y = ana_grid_y(i);
        s_row_color[y] = (i == 0 || i == (int)ANA_DIVS) ? C_EDGE :
                         (i == (int)ANA_DIVS / 2 ? C_GRID_MAJ : C_GRID);
    }
    for (i = 0; i < (int)DIG_LANES; i++)
        s_row_color[DIG_Y + i * DIG_H] = C_EDGE;

    /* 竖格线 */
    for (x = 0; x < (int)SCR_W; x++)
        s_vcol_color[x] = 0;

    for (i = 0; i <= (int)DIVX; i++)
    {
        x = i * (int)DW;
        if (x > (int)WAVE_W - 1) x = (int)WAVE_W - 1;
        s_vcol_color[x] = (i == 0 || i == (int)DIVX) ? C_GRID_MAJ : C_GRID;
    }
}

static void bg_at_update_trig(void)
{
    s_trig_y = ana_y(g_ui.trig_level);
}

/* ==========================================================================
 *  背景色查询：给定 (x,y) 返回"没有波形时这里应该是什么颜色"
 *  优先级和 draw_grid() / draw_trig() 的绘制顺序严格一致（后画的覆盖先画的）
 * ========================================================================== */
static uint16_t bg_at(int x, int y)
{
    uint16_t c;

    /* 1) 底色 = 该行的横格线颜色 */
    c = s_row_color[y];

    /* 2) 竖格线覆盖横线（但竖线只画在波形区 y 范围内） */
    if (s_vcol_color[x] != 0 && y >= (int)WV_Y && y < (int)WV_BOT)
        c = s_vcol_color[x];

    /* 3) 数字触发不画模拟电平线。 */
    if (g_ui.trig_src <= TRIGSRC_CH2)
    {
        if (y == s_trig_y && (x % 7) < 4)
            c = C_TRIG;
    }

    return c;
}

/* ==========================================================================
 *  5. 静态部分：网格 / 状态栏底 / 工具条
 * ========================================================================== */

static void draw_grid(void)
{
    int i, x, y;

    fillrect(0, (int)WV_Y, (int)WAVE_W, (int)WV_H, C_BG);

    for (i = 0; i <= (int)ANA_DIVS; i++) {
        y = ana_grid_y(i);
        hline_(0, WAVE_W - 1, y, s_row_color[y]);
    }
    for (i = 0; i < (int)DIG_LANES; i++)
        hline_(0, WAVE_W - 1, DIG_Y + i * DIG_H, C_EDGE);

    /* 竖格线贯穿五路，方便直接比较跳变沿。 */
    for (i = 0; i <= (int)DIVX; i++)
    {
        x = i * (int)DW;
        if (x > (int)WAVE_W - 1) x = (int)WAVE_W - 1;
        vline_(x, (int)WV_Y, (int)WV_BOT - 1,
               (i == 0 || i == (int)DIVX) ? C_GRID_MAJ : C_GRID);
    }
}

/* ==========================================================================
 *  底部栏（测量读数行 + 软键行）和状态栏已经拆到 app_screen.c：
 *      scr_status_bar()   scr_meas_line()   scr_softkeys()
 *  本文件只保留"波形区"相关的绘制：网格、轨迹、触发线、测量曲线。
 * ========================================================================== */

/* ==========================================================================
 *  6. 波形绘制
 * ========================================================================== */

static void trace_ana(const wave_window_t *w, const uint16_t *v, uint16_t color)
{
    int i, n = (int)w->n;
    int lx = -1, ly = 0;

    if (n > (int)NPT) n = (int)NPT;         /* 防越界：n 是协议给的 */

    for (i = 0; i < n; i++)
    {
        int x = x_of(i, w);
        int y = ana_y(v[i]);

        if (lx < 0)
        {
            px(x, y, color);
        }
        else
        {
            vline_(x, ly, y, color);                    /* 垂直连接前后点 */
            if (x > lx + 1) hline_(lx, x - 1, ly, color); /* 横向补缝 */
        }
        lx = x;
        ly = y;
    }

}

static void trace_dig(const wave_window_t *w, int lane, int bit, uint16_t color)
{
    int i, n = (int)w->n;
    int top = (int)DIG_Y + lane * (int)DIG_H;
    int yh = top + 4;
    int yl = top + (int)DIG_H - 4;
    int lx = -1, lv = 0;

    if (n > (int)NPT) n = (int)NPT;

    for (i = 0; i < n; i++)
    {
        int x = x_of(i, w);
        int v = (int)((w->dig[i] >> bit) & 1u);
        int y = v ? yh : yl;

        if (lx < 0)
        {
            px(x, y, color);
        }
        else if (v == lv)
        {
            hline_(lx, x, y, color);
        }
        else
        {
            hline_(lx, x, lv ? yh : yl, color);         /* 把上一段电平补到 x */
            vline_(x, yh, yl, color);                   /* 跳变沿 */
        }
        lx = x;
        lv = v;
    }

}

static void draw_trig(void)
{
    int i, y;

    /* 只有模拟触发源画电平线：数字通道是按逻辑电平触发的，没有电压阈值。
     * 也不再画"触发位置"竖直虚线 —— 采集端把触发点对齐到这一帧的起点，
     * 画在屏幕 30% 处会误导人。 */
    if (g_ui.trig_src > TRIGSRC_CH2) return;

    y = ana_y(g_ui.trig_level);
    for (i = 0; i < (int)WAVE_W; i += 7)
        hline_(i, i + 3, y, C_TRIG);
}

/* 五路使用同一帧和同一横坐标，显示开关彼此独立。 */
static void draw_traces(const wave_window_t *w, uint8_t mask)
{

    if (mask & CHM_CH1) trace_ana(w, w->ch1, C_CH1);
    if (mask & CHM_CH2) trace_ana(w, w->ch2, C_CH2);
    if (mask & CHM_DIG0) trace_dig(w, 0, 0, C_DIG0);
    if (mask & CHM_DIG1) trace_dig(w, 1, 1, C_DIG1);
    if (mask & CHM_DIG2) trace_dig(w, 2, 2, C_DIG2);
}

/* ==========================================================================
 *  7. 测量
 * ========================================================================== */

/* 模拟测量：平均值求电压，最大值减最小值求 Vpp，数上升沿求频率。 */
static void measure_analog(const uint16_t *samples, int count, float dt, ui_meas_t *result)
{
    int i, middle, rises = 0, first = 0, last = 0;
    uint16_t minimum = 4095, maximum = 0;
    uint32_t sum = 0;

    for (i = 0; i < count; i++) {
        if (samples[i] < minimum) minimum = samples[i];
        if (samples[i] > maximum) maximum = samples[i];
        sum += samples[i];
    }
    result->vavg = ui_code_to_volt((uint16_t)(sum / count));
    result->vpp = ui_code_to_volt(maximum - minimum);

    middle = (minimum + maximum) / 2;
    for (i = 1; i < count; i++) {
        if (samples[i - 1] <= middle && samples[i] > middle) {
            if (rises == 0) first = i;
            last = i;
            rises++;
        }
    }
    // 两个上升沿才有一个完整周期；多个周期一起平均。
    if (rises >= 2 && dt > 0) {
        result->freq = (rises - 1) / ((last - first) * dt);
        result->valid = 1;
    }
}

/* 数字测量：高电平点数占比是占空比，真实的 0->1 跳变用来求频率。 */
static void measure_digital(const uint8_t *samples, int count, float dt, int bit, ui_meas_t *result)
{
    int i, high_count = 0, rises = 0, first = 0, last = 0;
    uint8_t mask = (uint8_t)(1u << bit);

    for (i = 0; i < count; i++) {
        if (samples[i] & mask) high_count++;
        // 窗口第一个点是高电平时，不能凭空算作一次上升沿。
        if (i > 0 && !(samples[i - 1] & mask) && (samples[i] & mask)) {
            if (rises == 0) first = i;
            last = i;
            rises++;
        }
    }
    result->duty = (float)high_count / count;
    result->valid = 1;
    if (rises >= 2 && dt > 0) {
        result->freq = (rises - 1) / ((last - first) * dt);
        result->width = result->duty / result->freq; // 周期乘占空比，估算高电平宽度
    }
}

void ui_measure(const wave_window_t *w, ui_meas_all_t *m)
{
    int count;
    float dt;
    memset(m, 0, sizeof(*m));
    if (!w || w->n < 2) return;
    count = w->n;
    if (count > NPT) count = NPT;
    dt = w->dt_ns / 1000000000.0f; // 纳秒换成秒

    measure_analog(w->ch1, count, dt, &m->ch1);
    measure_analog(w->ch2, count, dt, &m->ch2);
    measure_digital(w->dig, count, dt, 0, &m->dig0); // D0 声音门限 PI0
    measure_digital(w->dig, count, dt, 1, &m->dig1); // D1 按键 PF8
    measure_digital(w->dig, count, dt, 2, &m->dig2); // D2 方波回环 PI2
}

/* 初始化三块 SDRAM 显存，或退回一块 SRAM 显存。 */
void ui_init(void)
{
    int i;
    memset(&g_ui, 0, sizeof(g_ui));
    s_triple = g_sdram_ok;
    if (s_triple) {
        s_fbs[0] = (uint16_t *)FB0_ADDR;
        s_fbs[1] = (uint16_t *)FB1_ADDR;
        s_fbs[2] = (uint16_t *)FB2_ADDR;
        s_fb_idx = 1;
    } else {
        s_fbs[0] = s_fbs[1] = s_fbs[2] = FB_SRAM;
        s_fb_idx = 0;
    }
    s_show = s_fbs[0];
    drv_lcd_init(s_show);
    bg_tables_init();
    bg_at_update_trig();

    // 先清空所有缓冲，避免第一次换页显示随机内容。
    for (i = 0; i < (s_triple ? 3 : 1); i++) {
        s_fb = s_fbs[i];
        fillrect(0, 0, SCR_W, SCR_H, C_BG);
    }
    s_fb = s_fbs[s_fb_idx];
    s_prev_valid = 0;
    s_redraw = 1;
    drv_lcd_set_fb(s_show);
    printf("ui: SDRAM=%d triple=%d fb=%p\r\n", g_sdram_ok, s_triple, (void *)s_fb);
}

/* 直接比较需要重画网格、按钮的配置，不再计算状态签名。 */
static uint8_t wave_settings_changed(const wave_window_t *w)
{
    return !s_prev_valid || s_prev.scan_code != w->scan_code ||
           s_last_ui.run != g_ui.run || s_last_ui.scan != g_ui.scan ||
           s_last_ui.ch_mask != g_ui.ch_mask ||
           s_last_ui.trig_level != g_ui.trig_level ||
           s_last_ui.trig_src != g_ui.trig_src ||
           s_last_ui.trig_edge != g_ui.trig_edge ||
           s_last_ui.trig_mode != g_ui.trig_mode;
}

/* 更新触摸状态；一次按下只允许一个按钮响应。 */
void ui_touch_set(int x, int y, int pressed)
{
    if (!pressed || !s_touch_down) s_touch_consumed = 0;
    s_touch_x = x;
    s_touch_y = y;
    s_touch_down = pressed;
}

int ui_toolbar_hit(int x, int y)
{
    if (s_page != UI_PAGE_WAVE) return UI_WID_NONE;
    return scr_softkey_hit(x, y);
}

/* 设置页按钮。按下状态只用于外观，一次按下只产生一次点击。 */
void ui_btn_init(ui_btn_t *b)
{
    static const ui_btn_t def = UI_BTN_DEFAULT;

    if (b != 0)
        *b = def;
}

void ui_btn_invalidate(ui_btn_t *b)
{
    if (b != 0)
        b->drawn = 0;
}

static void btn_paint(const ui_btn_t *button, int highlighted)
{
    uint16_t bg = highlighted ? C_TB_BTN_ON : C_TB_BTN;
    uint16_t fg = highlighted ? C_GREEN : C_TB_TX;
    fillrect(button->x, button->y, button->w, button->h, bg);
    rect_(button->x, button->y, button->w, button->h, highlighted ? C_GREEN : C_EDGE);
    if (button->label)
        txt_c(&Font12, button->x + button->w / 2, button->y + (button->h - F12H) / 2,
              button->label, fg, bg);
}

int ui_button(ui_btn_t *button, int x, int y, int w, int h, const char *label, int on)
{
    int inside, highlighted, clicked = 0;
    if (!button || w <= 0 || h <= 0) return 0;
    if (button->label != label || button->x != x || button->y != y ||
        button->w != w || button->h != h) button->drawn = 0;
    button->x = (int16_t)x;
    button->y = (int16_t)y;
    button->w = (int16_t)w;
    button->h = (int16_t)h;
    button->label = label;

    inside = s_touch_down && s_touch_x >= x && s_touch_x < x + w && s_touch_y >= y && s_touch_y < y + h;
    if (inside && !s_touch_consumed) {
        s_touch_consumed = 1;
        clicked = 1;
    }
    highlighted = on || inside;
    // 状态变化或换了显存，才需要重画按钮。
    if (!button->drawn || highlighted != button->last_vis || button->gen != s_draw_gen) {
        btn_paint(button, highlighted);
        button->drawn = 1;
        button->last_vis = (uint8_t)highlighted;
        button->gen = s_draw_gen;
    }
    return clicked;
}

/* ==========================================================================
 *  页面切换
 *
 *  波形页和设置页互斥：切页时让后台页面重绘，并把所有"内容没变就跳过"的缓存
 *  全部失效，保证新页面是从干净背景开始重画的。
 * ========================================================================== */
void ui_page_set(uint8_t page)
{
    page = (page == UI_PAGE_SETTINGS) ? UI_PAGE_SETTINGS : UI_PAGE_WAVE;

    if (page == s_page)
        return;

    s_page = page;
    s_touch_consumed = 1; /* 打开菜单的那次触摸不再点击菜单里的按钮。 */

    /* 下一次绘制只清后台缓冲，避免清掉 LTDC 正在显示的页面。 */
    ui_force_redraw();              /* 状态栏/工具条/数值域缓存全部失效 */

    if (page == UI_PAGE_WAVE)
        s_prev_valid = 0;           /* 回波形页：没有"上一帧"可擦，直接重画 */
}

uint8_t ui_page_get(void)
{
    return s_page;
}

void ui_draw(const wave_window_t *w)
{
    if (!s_fb) ui_init();

    if (s_page == UI_PAGE_SETTINGS) {
        if (s_triple || s_redraw) {
            fillrect(0, 0, SCR_W, SCR_H, C_BG);
            ui_field_reset();
            s_redraw = 0;
        }
        app_ui_draw_settings();
        flip();
        return;
    }
    if (!w || w->n < 2) return;

    if (s_triple) {
        // 后台缓冲整帧重画，完成后再交给屏幕。
        fillrect(0, 0, SCR_W, SCR_H, C_BG);
        draw_grid();
        draw_trig();
        scr_softkeys();
    } else {
        drv_lcd_wait_vsync();
        if (s_redraw || wave_settings_changed(w)) {
            bg_at_update_trig();
            draw_grid();
            draw_trig();
            scr_softkeys();
        } else if (s_prev_valid) {
            // 先擦完所有旧通道，再画新通道，避免互相擦掉。
            s_restore = 1;
            draw_traces(&s_prev, s_prev_mask);
            s_restore = 0;
        }
    }
    draw_traces(w, g_ui.ch_mask);
    scr_status_bar(w);
    scr_meas_line(w);
    s_redraw = 0;

    if (!s_triple) {
        s_prev = *w;
        s_prev_mask = g_ui.ch_mask;
        s_prev_valid = 1;
        s_last_ui = g_ui;
    }
    flip();
}

/* 字符串格式化与基本绘图接口。 */
char *ui_str(char *p, const char *s)
{
    return put_s(p, s);
}

char *ui_itoa(char *p, int32_t v)
{
    uint32_t u;
    char     t[12];
    int      n = 0;

    if (p == 0) return 0;

    if (v < 0)
    {
        *p++ = '-';
        u = (uint32_t)(-v);
    }
    else
    {
        u = (uint32_t)v;
    }

    do { t[n++] = (char)('0' + (u % 10u)); u /= 10u; } while (u != 0u && n < 12);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

char *ui_fix(char *p, int32_t v, int dec)
{
    return put_fix(p, v, dec);
}

/* 实心矩形 (x, y) 左上角，宽 w 高 h */
void ui_fill(int x, int y, int w, int h, uint16_t color)
{
    fillrect(x, y, w, h, color);
}

void ui_hline(int x0, int x1, int y, uint16_t color)
{
    hline_(x0, x1, y, color);
}

/* ---- 文字 ---- */

int ui_text(int x, int y, const char *s, uint16_t fg, int32_t bg)
{
    txt(&Font12, x, y, s, fg, bg);
    return tw(&Font12, s);
}

int ui_text_w(const char *s)
{
    return tw(&Font12, s);
}

void ui_text_c(int cx, int y, const char *s, uint16_t fg, int32_t bg)
{
    txt(&Font12, cx - tw(&Font12, s) / 2, y, s, fg, bg);
}

/* ==========================================================================
 *  10. 数据刷新
 * ========================================================================== */

/* 一块矩形里擦干净再居中画一行字。每帧调也安全 */
int ui_text_at(int x, int y, int w, int h, const char *s, uint16_t fg, uint16_t bg)
{
    int wpx = tw(&Font12, s);

    if (wpx > 0) wpx -= 1;                  /* 去掉最后那个字间距，看起来才真居中 */

    fillrect(x, y, w, h, bg);               /* 先擦 */
    txt(&Font12, x + (w - wpx) / 2, y + (h - F12H) / 2, s, fg, (int32_t)bg);
    return wpx;
}

/* ui_field 的上一次内容缓存 */
static char s_field_last[UI_FIELD_MAX][24];

void ui_field_reset(void)
{
    memset(s_field_last, 0, sizeof(s_field_last));
}

uint8_t ui_field(uint8_t id, int x, int y, int w, int h,
                 const char *s, uint16_t fg, uint16_t bg)
{
    char tmp[24];
    int  i, wpx;

    if (id >= UI_FIELD_MAX || s == 0)
        return 0;

    /* 截断到 23 字符 */
    memset(tmp, 0, sizeof(tmp));
    for (i = 0; i < 23 && s[i] != 0; i++)
        tmp[i] = s[i];

    /* 内容一模一样，直接返回 —— 不写一个像素，也就绝不会闪 */
    if (strcmp(tmp, s_field_last[id]) == 0)
        return 0;

    memcpy(s_field_last[id], tmp, sizeof(tmp));

    wpx = tw(&Font12, tmp);
    if (wpx > 0) wpx -= 1;

    fillrect(x, y, w, h, bg);               /* 擦旧值 */
    txt(&Font12, x + (w - wpx) / 2, y + (h - F12H) / 2, tmp, fg, (int32_t)bg);
    return 1;
}

void ui_force_redraw(void)
{
    s_draw_gen++; /* 背景重画后，按钮缓存也必须失效。 */
    s_redraw = 1;               /* 下一次 ui_draw 会重画状态栏、按钮和网格 */
    ui_field_reset();               /* 区域被清了，数值缓存也要失效 */
}
