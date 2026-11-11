/* gfx_draw.c：基础绘图：像素、线、矩形、字库、数字格式化 */

#include "app_gfx.h"
#include "app_layout.h"
#include "drv_font.h"
#include "gfx_priv.h"
#include <string.h>

uint8_t s_restore;

void px(int x, int y, uint16_t c)
{

	if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
	{
		if (s_restore != 0) {
			s_fb[(uint32_t)y * SCR_W + (uint32_t)x] = bg_at(x, y);
		} else {
			s_fb[(uint32_t)y * SCR_W + (uint32_t)x] = c;
		}
	}
}

void fillrect(int x, int y, int w, int h, uint16_t c)
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

void hline_(int x0, int x1, int y, uint16_t c)
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

			if (s_restore != 0) {
			    row[i] = bg_at(i, y);
			} else {
			    row[i] = c;
			}
	}
}

void vline_(int x, int y0, int y1, uint16_t c)
{
	int i, t;

	if (x < 0 || x >= (int)SCR_W) return;
	if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
	if (y0 < 0) y0 = 0;
	if (y1 > (int)SCR_H - 1) y1 = (int)SCR_H - 1;
	if (y1 < y0) return;

	for (i = y0; i <= y1; i++)
	{

			if (s_restore != 0) {
			    s_fb[(uint32_t)i * SCR_W + (uint32_t)x] = bg_at(x, i);
			} else {
			    s_fb[(uint32_t)i * SCR_W + (uint32_t)x] = c;
			}
	}
}

void rect_(int x, int y, int w, int h, uint16_t c)
{
	hline_(x, x + w - 1, y, c);
	hline_(x, x + w - 1, y + h - 1, c);
	vline_(x, y, y + h - 1, c);
	vline_(x + w - 1, y, y + h - 1, c);
}

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

		for (c = 0; c <= (int)f->Width; c++)
		{
			int xx = x + c;
			int ink;
			uint8_t bit_mask;

			if (xx < 0 || xx >= (int)SCR_W) continue;

			if (c == (int)f->Width)
			{
				ink = 0;
			}
			else
			{
				uint8_t b = p[r * bpr + (c >> 3)];
				bit_mask = (uint8_t)(0x80 >> (c & 7));
				if ((b & bit_mask) != 0) {
				    ink = 1;
				} else {
				    ink = 0;
				}
			}

			if (ink)
				s_fb[(uint32_t)yy * SCR_W + (uint32_t)xx] = fg;
			else if (bg >= 0)
				s_fb[(uint32_t)yy * SCR_W + (uint32_t)xx] = (uint16_t)bg;
		}
	}
}

int tw(const sFONT *f, const char *s)
{
	return (int)strlen(s) * ((int)f->Width + 1);
}

void txt(const sFONT *f, int x, int y, const char *s, uint16_t fg, int32_t bg)
{
	while (*s)
	{
		glyph(f, x, y, *s, fg, bg);
		x += (int)f->Width + 1;
		s++;
	}
}

void txt_c(const sFONT *f, int cx, int y, const char *s, uint16_t fg, int32_t bg)
{
	txt(f, cx - tw(f, s) / 2, y, s, fg, bg);
}

char *put_u(char *p, uint32_t v)
{
	char t[11];
	int n = 0;

	do { t[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v && n < 11);
	while (n) *p++ = t[--n];
	*p = 0;
	return p;
}

char *put_fix(char *p, int32_t v, int dec)
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

char *put_s(char *p, const char *s)
{
	while (*s) *p++ = *s++;
	*p = 0;
	return p;
}

void ui_fill(int x, int y, int w, int h, uint16_t color)
{
	fillrect(x, y, w, h, color);
}

void ui_hline(int x0, int x1, int y, uint16_t color)
{
	hline_(x0, x1, y, color);
}

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
