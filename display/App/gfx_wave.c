/* gfx_wave.c：波形绘制：网格、坐标换算、模拟与数字波形 */

#include "app_gfx.h"
#include "app_layout.h"
#include "gfx_priv.h"
#include <string.h>

static const uint32_t s_div_ns[5]  = SCOPE_DIV_NS;
static const char    *s_div_lab[5] = SCOPE_DIV_LABELS;

uint32_t ui_scan_div_ns(uint8_t scan)
{
	if (scan > 4u) {
	    scan = 4u;
	}
	return s_div_ns[scan];
}

const char *ui_scan_label(uint8_t scan)
{
	if (scan > 4u) {
	    scan = 4u;
	}
	return s_div_lab[scan];
}

float ui_code_to_volt(uint16_t code)
{
	return (float)code * 3.3f / 4095.0f;
}

static int ana_grid_y(int i)
{
    return (int)ANA_Y + i * ((int)ANA_H - 1) / (int)ANA_DIVS;
}

int ana_y(uint16_t code)
{
    if (code > 4095u) code = 4095u;
    return (int)ANA_Y + (int)ANA_H - 1 - (int)((uint32_t)code * (ANA_H - 1u) / 4095u);
}

int x_of(int i, const wave_window_t *w)
{
	if (w->n <= 1) return 0;
    if (w->dt_ns == ui_scan_div_ns(w->scan_code) / PPD)
        return i * (int)DW / (int)PPD;
    return (int)((uint64_t)i * w->dt_ns * DW / ui_scan_div_ns(w->scan_code));
}

static uint16_t s_row_color[SCR_H];
static uint16_t s_vcol_color[SCR_W];
static int      s_trig_y;

void bg_tables_init(void)
{
	int i, y, x;

	for (y = 0; y < (int)SCR_H; y++)
		s_row_color[y] = C_BG;

    for (i = 0; i <= (int)ANA_DIVS; i++) {
        y = ana_grid_y(i);
        if (i == 0 || i == (int)ANA_DIVS) {
            s_row_color[y] = C_EDGE;
        } else if (i == (int)ANA_DIVS / 2) {
            s_row_color[y] = C_GRID_MAJ;
        } else {
            s_row_color[y] = C_GRID;
        }
    }
    for (i = 0; i < (int)DIG_LANES; i++)
        s_row_color[DIG_Y + i * DIG_H] = C_EDGE;

	for (x = 0; x < (int)SCR_W; x++)
		s_vcol_color[x] = 0;

	for (i = 0; i <= (int)DIVX; i++)
	{
		x = i * (int)DW;
		if (x > (int)WAVE_W - 1) x = (int)WAVE_W - 1;
		if (i == 0 || i == (int)DIVX) {
		    s_vcol_color[x] = C_GRID_MAJ;
		} else {
		    s_vcol_color[x] = C_GRID;
		}
	}
}

void bg_at_update_trig(void)
{
	s_trig_y = ana_y(g_ui.trig_level);
}

uint16_t bg_at(int x, int y)
{
	uint16_t c;

	c = s_row_color[y];

	if (s_vcol_color[x] != 0 && y >= (int)WV_Y && y < (int)WV_BOT)
		c = s_vcol_color[x];

	if (g_ui.trig_src <= TRIGSRC_CH2)
	{
		if (y == s_trig_y && (x % 7) < 4)
			c = C_TRIG;
	}

	return c;
}

void draw_grid(void)
{
	int i, x, y;

	fillrect(0, (int)WV_Y, (int)WAVE_W, (int)WV_H, C_BG);

    for (i = 0; i <= (int)ANA_DIVS; i++) {
        y = ana_grid_y(i);
        hline_(0, WAVE_W - 1, y, s_row_color[y]);
    }
    for (i = 0; i < (int)DIG_LANES; i++)
        hline_(0, WAVE_W - 1, DIG_Y + i * DIG_H, C_EDGE);

	for (i = 0; i <= (int)DIVX; i++)
	{
		x = i * (int)DW;
		if (x > (int)WAVE_W - 1) x = (int)WAVE_W - 1;
		if (i == 0 || i == (int)DIVX) {
			vline_(x, (int)WV_Y, (int)WV_BOT - 1, C_GRID_MAJ);
		} else {
			vline_(x, (int)WV_Y, (int)WV_BOT - 1, C_GRID);
		}
	}
}

void trace_ana(const wave_window_t *w, const uint16_t *v, uint16_t color)
{
	int i, n = (int)w->n;
	int lx = -1, ly = 0;

	if (n > (int)NPT) n = (int)NPT;

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
			vline_(x, ly, y, color);
			if (x > lx + 1) hline_(lx, x - 1, ly, color);
		}
		lx = x;
		ly = y;
	}

}

void trace_dig(const wave_window_t *w, int lane, int bit, uint16_t color)
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
		int y;
		if (v != 0) {
		    y = yh;
		} else {
		    y = yl;
		}

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
			if (lv != 0) {
				hline_(lx, x, yh, color);
			} else {
				hline_(lx, x, yl, color);
			}
			vline_(x, yh, yl, color);
		}
		lx = x;
		lv = v;
	}

}

void draw_trig(void)
{
	int i, y;

	if (g_ui.trig_src > TRIGSRC_CH2) return;

	y = ana_y(g_ui.trig_level);
	for (i = 0; i < (int)WAVE_W; i += 7)
		hline_(i, i + 3, y, C_TRIG);
}

void draw_traces(const wave_window_t *w)
{

    if (g_ui.ch_mask & CHM_CH1) trace_ana(w, w->ch1, C_CH1);
    if (g_ui.ch_mask & CHM_CH2) trace_ana(w, w->ch2, C_CH2);
    if (g_ui.ch_mask & CHM_DIG0) trace_dig(w, 0, 0, C_DIG0);
    if (g_ui.ch_mask & CHM_DIG1) trace_dig(w, 1, 1, C_DIG1);
    if (g_ui.ch_mask & CHM_DIG2) trace_dig(w, 2, 2, C_DIG2);
}
