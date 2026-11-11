/* gfx_meas.c：自动测量：频率、峰峰值、占空比 */

#include "app_gfx.h"
#include "app_layout.h"
#include "gfx_priv.h"
#include <string.h>

static void meas_ana(const uint16_t *v, uint16_t n, float dt, ui_meas_t *r)
{
	int i, mid, edges = 0, first = -1, last = -1;
	uint16_t vmin = 4095u, vmax = 0u;
	uint32_t sum = 0;

	memset(r, 0, sizeof(*r));
	if (n < 2) return;

	for (i = 0; i < (int)n; i++)
	{
		if (v[i] < vmin) vmin = v[i];
		if (v[i] > vmax) vmax = v[i];
		sum += v[i];
	}

	mid = ((int)vmin + (int)vmax) / 2;
	for (i = 1; i < (int)n; i++)
	{
		if (v[i - 1] <= mid && v[i] > mid)
		{
			if (first < 0) first = i;
			else           last = i;
			edges++;
		}
	}

	r->vpp  = ui_code_to_volt((uint16_t)(vmax - vmin));
	r->vavg = ui_code_to_volt((uint16_t)(sum / n));

	if (edges >= 2 && last > first && dt > 0.0f)
	{
		r->freq  = (float)(edges - 1) / ((float)(last - first) * dt);
		r->valid = 1;
	}
}

static void meas_dig(const uint8_t *d, uint16_t n, float dt, int bit, ui_meas_t *r)
{
	int i, high = 0, rises = 0, first = -1, last = -1, prev = 0;

	memset(r, 0, sizeof(*r));
	if (n < 2) return;

	for (i = 0; i < (int)n; i++)
	{
		int v = (int)((d[i] >> bit) & 1u);
		if (v) high++;
		if (v && !prev)
		{
			if (first < 0) first = i;
			else           last = i;
			rises++;
		}
		prev = v;
	}

	r->duty = (float)high / (float)n;
	if (rises >= 2 && last > first && dt > 0.0f)
	{
		r->freq  = (float)(rises - 1) / ((float)(last - first) * dt);
		r->width = r->duty / r->freq;
	}
	r->valid = 1;
}

void ui_measure(const wave_window_t *w, ui_meas_all_t *m)
{
	uint16_t n;
	float dt;

	memset(m, 0, sizeof(*m));
	if (w == 0 || w->n < 2) return;

	n = w->n;
	if (n > NPT) n = NPT;
	dt = (float)w->dt_ns * 1e-9f;

	meas_ana(w->ch1, n, dt, &m->ch1);
	meas_ana(w->ch2, n, dt, &m->ch2);
	meas_dig(w->dig, n, dt, 0, &m->dig0);
	meas_dig(w->dig, n, dt, 1, &m->dig1);
	meas_dig(w->dig, n, dt, 2, &m->dig2);
}
