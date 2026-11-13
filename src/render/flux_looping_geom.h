#ifndef FLUX_LOOPING_GEOM_H
#define FLUX_LOOPING_GEOM_H

#include "fluxent/flux_types.h"
#include "controls/data/flux_looping_data.h"
#include "render/flux_scroll_geom.h"

#include <math.h>

typedef struct FluxLoopingMetrics {
	float item_height;
	float viewport_h;
	int   slots;
} FluxLoopingMetrics;

static float inline flux_looping_pad(FluxLoopingMetrics const *m) {
	float pad = (m->viewport_h - m->item_height) * 0.5f;
	return pad > 0.0f ? pad : 0.0f;
}

static float inline flux_looping_content_h(FluxLoopingMetrics const *m) {
	return 2.0f * flux_looping_pad(m) + ( float ) m->slots * m->item_height;
}

static float inline flux_looping_item_y(FluxLoopingMetrics const *m, int visual) {
	return flux_looping_pad(m) + ( float ) visual * m->item_height;
}

static int inline flux_looping_visual_at(FluxLoopingMetrics const *m, float offset) {
	if (m->item_height <= 0.0f) return 0;
	return ( int ) floorf(offset / m->item_height + 0.5f);
}

static int inline flux_looping_first_visible(FluxLoopingMetrics const *m, float offset) {
	if (m->item_height <= 0.0f) return 0;
	return ( int ) floorf((offset - flux_looping_pad(m)) / m->item_height);
}

static int inline flux_looping_window_slots(FluxLoopingMetrics const *m) {
	if (m->item_height <= 0.0f || m->viewport_h <= 0.0f) return 0;
	return ( int ) ceilf(m->viewport_h / m->item_height) + 2;
}

static int inline flux_looping_mod(int x, int n) { return n > 0 ? (x % n + n) % n : 0; }

static FluxRect inline flux_looping_up_rect(FluxRect const *bar) {
	float h = bar->h < FLUX_LOOPING_BUTTON_H ? bar->h : FLUX_LOOPING_BUTTON_H;
	return ( FluxRect ) {bar->x, bar->y, bar->w, h};
}

static FluxRect inline flux_looping_down_rect(FluxRect const *bar) {
	float h = bar->h < FLUX_LOOPING_BUTTON_H ? bar->h : FLUX_LOOPING_BUTTON_H;
	return ( FluxRect ) {bar->x, bar->y + bar->h - h, bar->w, h};
}

#endif
