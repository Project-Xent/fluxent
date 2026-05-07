/**
 * @file flux_scroll_geom.h
 * @brief Shared geometry helpers for ScrollView overlay scrollbars.
 */

#ifndef FLUX_SCROLL_GEOM_H
#define FLUX_SCROLL_GEOM_H

#include "fluxent/flux_types.h"
#include <stdbool.h>
#include <string.h>

#define FLUX_SCROLLBAR_FULL_SIZE    12.0f
#define FLUX_SCROLLBAR_MINI_SIZE    2.0f
#define FLUX_SCROLLBAR_BUTTON_SIZE  12.0f
#define FLUX_SCROLLBAR_MIN_THUMB    30.0f
#define FLUX_SCROLLBAR_SMALL_CHANGE 48.0f

/** @brief Computed rectangles for a ScrollView overlay scrollbar pair. */
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

static float inline flux_scroll_thumb_size(float track_len, float viewport_len, float content_len) {
	float ratio = viewport_len / content_len;
	if (ratio > 1.0f) ratio = 1.0f;

	float min_len = track_len < FLUX_SCROLLBAR_MIN_THUMB ? track_len : FLUX_SCROLLBAR_MIN_THUMB;
	float len     = track_len * ratio;
	if (len < min_len) len = min_len;
	if (len > track_len) len = track_len;
	return len;
}

static float inline flux_scroll_thumb_offset(
  float track_len, float thumb_len, float viewport_len, float content_len, float scroll
) {
	float max_scroll = content_len - viewport_len;
	float pct        = max_scroll > 0.0f ? scroll / max_scroll : 0.0f;
	pct              = flux_scroll_clampf(pct, 0.0f, 1.0f);
	return (track_len - thumb_len) * pct;
}

static void inline flux_scroll_geom_vertical(
  FluxScrollBarGeom *g, FluxRect const *bounds, float content_h, float scroll_y, float v_bar_h
) {
	float bx      = bounds->x + bounds->w - g->bar_w;
	float by      = bounds->y;
	float btn_h   = FLUX_SCROLLBAR_BUTTON_SIZE * g->expanded;

	g->v_bar      = ( FluxRect ) {bx, by, g->bar_w, v_bar_h};
	g->v_up_btn   = ( FluxRect ) {bx, by, g->bar_w, btn_h};
	g->v_dn_btn   = ( FluxRect ) {bx, by + v_bar_h - btn_h, g->bar_w, btn_h};

	float track_y = by + btn_h;
	float track_h = v_bar_h - 2.0f * btn_h;
	if (track_h < 0.0f) track_h = 0.0f;
	g->v_track    = ( FluxRect ) {bx, track_y, g->bar_w, track_h};

	float thumb_h = flux_scroll_thumb_size(track_h, bounds->h, content_h);
	float thumb_y = track_y + flux_scroll_thumb_offset(track_h, thumb_h, bounds->h, content_h, scroll_y);
	g->v_thumb    = ( FluxRect ) {bx, thumb_y, g->bar_w, thumb_h};
}

static void inline flux_scroll_geom_horizontal(
  FluxScrollBarGeom *g, FluxRect const *bounds, float content_w, float scroll_x, float h_bar_w
) {
	float bx      = bounds->x;
	float by      = bounds->y + bounds->h - g->bar_w;
	float btn_w   = FLUX_SCROLLBAR_BUTTON_SIZE * g->expanded;

	g->h_bar      = ( FluxRect ) {bx, by, h_bar_w, g->bar_w};
	g->h_lf_btn   = ( FluxRect ) {bx, by, btn_w, g->bar_w};
	g->h_rg_btn   = ( FluxRect ) {bx + h_bar_w - btn_w, by, btn_w, g->bar_w};

	float track_x = bx + btn_w;
	float track_w = h_bar_w - 2.0f * btn_w;
	if (track_w < 0.0f) track_w = 0.0f;
	g->h_track    = ( FluxRect ) {track_x, by, track_w, g->bar_w};

	float thumb_w = flux_scroll_thumb_size(track_w, bounds->w, content_w);
	float thumb_x = track_x + flux_scroll_thumb_offset(track_w, thumb_w, bounds->w, content_w, scroll_x);
	g->h_thumb    = ( FluxRect ) {thumb_x, by, thumb_w, g->bar_w};
}

/** @brief Compute overlay scrollbar geometry for a ScrollView viewport. */
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

#endif /* FLUX_SCROLL_GEOM_H */
