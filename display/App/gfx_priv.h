#ifndef __GFX_PRIV_H
#define __GFX_PRIV_H

#include <stdint.h>
#include "app_gfx.h"
#include "drv_font.h"

#define F12H    12

extern uint16_t *s_fb;
extern uint8_t   s_double;

extern uint8_t   s_restore;

void px(int x, int y, uint16_t c);
void fillrect(int x, int y, int w, int h, uint16_t c);
void hline_(int x0, int x1, int y, uint16_t c);
void vline_(int x, int y0, int y1, uint16_t c);
void rect_(int x, int y, int w, int h, uint16_t c);

int  tw(const sFONT *f, const char *s);
void txt(const sFONT *f, int x, int y, const char *s, uint16_t fg, int32_t bg);
void txt_c(const sFONT *f, int cx, int y, const char *s, uint16_t fg, int32_t bg);

char *put_u(char *p, uint32_t v);
char *put_fix(char *p, int32_t v, int dec);
char *put_s(char *p, const char *s);

int      ana_y(uint16_t code);
int      x_of(int i, const wave_window_t *w);
uint16_t bg_at(int x, int y);
void     bg_tables_init(void);
void     bg_at_update_trig(void);
void     draw_grid(void);
void     draw_trig(void);
void     draw_traces(const wave_window_t *w);
void     trace_ana(const wave_window_t *w, const uint16_t *v, uint16_t color);
void     trace_dig(const wave_window_t *w, int lane, int bit, uint16_t color);

#endif
