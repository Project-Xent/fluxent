#ifndef FLUX_SCROLL_GEOM_H
#define FLUX_SCROLL_GEOM_H

#include "fluxent/flux_types.h"
#include <stdbool.h>
#include <string.h>

#define FLUX_SCROLLBAR_FULL_SIZE   12.0f
#define FLUX_SCROLLBAR_MINI_SIZE   2.0f
#define FLUX_SCROLLBAR_BUTTON_SIZE 12.0f
#ifdef FLUX_SCROLLBAR_MIN_THUMB
  #undef FLUX_SCROLLBAR_MIN_THUMB
#endif
#define FLUX_SCROLLBAR_MIN_THUMB    30.0f
#define FLUX_SCROLLBAR_SMALL_CHANGE 48.0f
#define FLUX_SCROLLBAR_THUMB_THIN   8.0f
#define FLUX_SCROLLBAR_THUMB_OFFSET 2.0f

typedef struct FluxScrollBarModel {
	float minimum;
	float maximum;
	float value;
	float viewport;
	bool  vertical;
} FluxScrollBarModel;

typedef struct FluxScrollBarParts {
	FluxRect bar;
	FluxRect dec_btn;
	FluxRect inc_btn;
	FluxRect track;
	FluxRect thumb;
	FluxRect large_dec;
	FluxRect large_inc;
	bool     thumb_visible;
} FluxScrollBarParts;

typedef struct FluxScrollBarGeom {
	bool     has_v;
	bool     has_h;
	float    bar_w;
	float    expanded;
	FluxRect v_bar;
	FluxRect v_up_btn;
	FluxRect v_dn_btn;
	FluxRect v_track;
	FluxRect v_thumb;
	FluxRect h_bar;
	FluxRect h_lf_btn;
	FluxRect h_rg_btn;
	FluxRect h_track;
	FluxRect h_thumb;
} FluxScrollBarGeom;

typedef struct FluxScrollGeomInput {
	FluxRect const *bounds;
	float           content_w;
	float           content_h;
	float           scroll_x;
	float           scroll_y;
	float           expanded;
} FluxScrollGeomInput;

static bool inline flux_rect_contains(FluxRect const *r, float x, float y) {
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static float inline flux_scroll_clampf(float value, float min, float max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

static float inline flux_scroll_thumb_size(float track_len, float viewport_len, float range) {
	float divisor = viewport_len + range;
	if (divisor < 1.0f) divisor = 1.0f;

	float len = track_len * viewport_len / divisor;
	if (len < FLUX_SCROLLBAR_MIN_THUMB) len = FLUX_SCROLLBAR_MIN_THUMB;
	if (len > track_len) len = track_len;
	return len;
}

static float inline flux_scroll_thumb_offset(float track_len, float thumb_len, float range, float value_off) {
	float pct = range > 0.0f ? value_off / range : 0.0f;
	pct       = flux_scroll_clampf(pct, 0.0f, 1.0f);
	return (track_len - thumb_len) * pct;
}

static void inline flux_scroll_span(FluxRect *r, FluxRect const *bar, bool vertical, float start, float len) {
	*r = *bar;
	if (vertical) {
		r->y = start;
		r->h = len;
		return;
	}
	r->x = start;
	r->w = len;
}

static void inline flux_scroll_bar_parts(
  FluxScrollBarParts *p, FluxRect const *bar, FluxScrollBarModel const *m, float expanded
) {
	bool  vertical  = m->vertical;
	float origin    = vertical ? bar->y : bar->x;
	float len       = vertical ? bar->h : bar->w;
	float btn_len   = FLUX_SCROLLBAR_BUTTON_SIZE * flux_scroll_clampf(expanded, 0.0f, 1.0f);
	float track_len = len - 2.0f * btn_len;
	if (track_len < 0.0f) track_len = 0.0f;

	float range      = m->maximum - m->minimum;
	float thumb_len  = flux_scroll_thumb_size(track_len, m->viewport, range);
	float thumb_off  = flux_scroll_thumb_offset(track_len, thumb_len, range, m->value - m->minimum);
	float track_beg  = origin + btn_len;
	float thumb_beg  = track_beg + thumb_off;

	p->bar           = *bar;
	p->thumb_visible = range > 0.0f && track_len > FLUX_SCROLLBAR_MIN_THUMB;
	flux_scroll_span(&p->dec_btn, bar, vertical, origin, btn_len);
	flux_scroll_span(&p->inc_btn, bar, vertical, origin + len - btn_len, btn_len);
	flux_scroll_span(&p->track, bar, vertical, track_beg, track_len);
	flux_scroll_span(&p->thumb, bar, vertical, thumb_beg, thumb_len);
	flux_scroll_span(&p->large_dec, bar, vertical, track_beg, thumb_off);
	flux_scroll_span(&p->large_inc, bar, vertical, thumb_beg + thumb_len, track_len - thumb_off - thumb_len);
}

static FluxScrollBarModel inline flux_scroll_geom_model(
  float viewport_len, float content_len, float scroll, bool vertical
) {
	float range = content_len - viewport_len;
	if (range < 0.0f) range = 0.0f;
	return ( FluxScrollBarModel ) {0.0f, range, scroll, viewport_len, vertical};
}

static void inline flux_scroll_geom_vertical(
  FluxScrollBarGeom *g, FluxRect const *bounds, float content_h, float scroll_y, float v_bar_h
) {
	FluxRect           bar   = {bounds->x + bounds->w - g->bar_w, bounds->y, g->bar_w, v_bar_h};
	FluxScrollBarModel m     = flux_scroll_geom_model(bounds->h, content_h, scroll_y, true);
	FluxScrollBarParts parts = {0};
	flux_scroll_bar_parts(&parts, &bar, &m, g->expanded);

	g->v_bar    = parts.bar;
	g->v_up_btn = parts.dec_btn;
	g->v_dn_btn = parts.inc_btn;
	g->v_track  = parts.track;
	g->v_thumb  = parts.thumb;
}

static void inline flux_scroll_geom_horizontal(
  FluxScrollBarGeom *g, FluxRect const *bounds, float content_w, float scroll_x, float h_bar_w
) {
	FluxRect           bar   = {bounds->x, bounds->y + bounds->h - g->bar_w, h_bar_w, g->bar_w};
	FluxScrollBarModel m     = flux_scroll_geom_model(bounds->w, content_w, scroll_x, false);
	FluxScrollBarParts parts = {0};
	flux_scroll_bar_parts(&parts, &bar, &m, g->expanded);

	g->h_bar    = parts.bar;
	g->h_lf_btn = parts.dec_btn;
	g->h_rg_btn = parts.inc_btn;
	g->h_track  = parts.track;
	g->h_thumb  = parts.thumb;
}

static void inline flux_scroll_geom_compute(FluxScrollBarGeom *g, FluxScrollGeomInput const *input) {
	FluxRect const *bounds = input->bounds;
	memset(g, 0, sizeof(*g));
	g->expanded   = flux_scroll_clampf(input->expanded, 0.0f, 1.0f);
	g->bar_w      = FLUX_SCROLLBAR_MINI_SIZE + (FLUX_SCROLLBAR_FULL_SIZE - FLUX_SCROLLBAR_MINI_SIZE) * g->expanded;
	g->has_v      = (input->content_h > bounds->h) && (input->content_h > 0.0f) && (bounds->h > 0.0f);
	g->has_h      = (input->content_w > bounds->w) && (input->content_w > 0.0f) && (bounds->w > 0.0f);

	float v_bar_h = bounds->h - (g->has_h ? g->bar_w : 0.0f);
	float h_bar_w = bounds->w - (g->has_v ? g->bar_w : 0.0f);
	if (g->has_v) flux_scroll_geom_vertical(g, bounds, input->content_h, input->scroll_y, v_bar_h);
	if (g->has_h) flux_scroll_geom_horizontal(g, bounds, input->content_w, input->scroll_x, h_bar_w);
}

#endif
