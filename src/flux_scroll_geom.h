/**
 * @file flux_scroll_geom.h
 * @brief Shared geometry helpers for the ScrollView overlay scrollbar.
 *
 * Used by both:
 *   - src/controls/flux_scroll.c  (rendering the bar)
 *   - src/input/flux_input.c      (hit-testing clicks / dragging the thumb)
 *
 * Mirrors the WinUI ScrollBar ControlTemplate:
 *   - ScrollBarSize             = 12 DIPs  (expanded track width)
 *   - Mini (panning) thumb width =  2 DIPs
 *   - LineUp / LineDown button   = 12 x 12
 *   - Thumb min length           = 30 DIPs
 *
 * @see microsoft-ui-xaml/src/controls/dev/CommonStyles/ScrollBar_themeresources.xaml
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
#define FLUX_SCROLLBAR_SMALL_CHANGE 48.0f /* LineUp / LineDown step */

typedef struct FluxScrollBarGeom {
	bool     has_v;
	bool     has_h;
	float    bar_w;    /* current track thickness (lerp mini->full) */
	float    expanded; /* 0..1 (0=mini panning indicator, 1=full bar) */

	/* Vertical bar (right edge) */
	FluxRect v_bar;    /* full bar rectangle */
	FluxRect v_up_btn; /* LineUp arrow button (may be zero-sized when collapsed) */
	FluxRect v_dn_btn; /* LineDown arrow button */
	FluxRect v_track;  /* track area between the two buttons */
	FluxRect v_thumb;  /* draggable thumb, positioned within v_track */

	/* Horizontal bar (bottom edge) */
	FluxRect h_bar;
	FluxRect h_lf_btn;
	FluxRect h_rg_btn;
	FluxRect h_track;
	FluxRect h_thumb;
} FluxScrollBarGeom;

static bool inline flux_rect_contains(FluxRect const *r, float x, float y) {
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

/* Compute scrollbar geometry for a ScrollView with viewport `bounds`,
 * content of size content_w x content_h, currently scrolled to (scroll_x,
 * scroll_y), rendered at expansion level `expanded` (0 = mini 2px indicator
 * with buttons collapsed; 1 = full 12px bar with visible arrow buttons). */
static void inline flux_scroll_geom_compute(
  FluxScrollBarGeom *g, FluxRect const *bounds, float content_w, float content_h, float scroll_x, float scroll_y,
  float expanded
) {
	memset(g, 0, sizeof(*g));
	if (expanded < 0.0f) expanded = 0.0f;
	if (expanded > 1.0f) expanded = 1.0f;
	g->expanded   = expanded;

	float bar_w   = FLUX_SCROLLBAR_MINI_SIZE + (FLUX_SCROLLBAR_FULL_SIZE - FLUX_SCROLLBAR_MINI_SIZE) * expanded;
	g->bar_w      = bar_w;

	g->has_v      = (content_h > bounds->h) && (content_h > 0.0f) && (bounds->h > 0.0f);
	g->has_h      = (content_w > bounds->w) && (content_w > 0.0f) && (bounds->w > 0.0f);

	/* Reserve the corner for the other scrollbar when both are visible. */
	float v_bar_h = bounds->h - (g->has_h ? bar_w : 0.0f);
	float h_bar_w = bounds->w - (g->has_v ? bar_w : 0.0f);

		if (g->has_v) {
			float bx      = bounds->x + bounds->w - bar_w;
			float by      = bounds->y;
			g->v_bar      = ( FluxRect ) {bx, by, bar_w, v_bar_h};

			/* Arrow buttons only materialize when bar is expanded (WinUI's mini
			 * panning indicator has no buttons).  Fade their area in with expand. */
			float btn_h   = FLUX_SCROLLBAR_BUTTON_SIZE * expanded;
			g->v_up_btn   = ( FluxRect ) {bx, by, bar_w, btn_h};
			g->v_dn_btn   = ( FluxRect ) {bx, by + v_bar_h - btn_h, bar_w, btn_h};

			float track_y = by + btn_h;
			float track_h = v_bar_h - 2.0f * btn_h;
			if (track_h < 0.0f) track_h = 0.0f;
			g->v_track  = ( FluxRect ) {bx, track_y, bar_w, track_h};

			float ratio = bounds->h / content_h;
			if (ratio > 1.0f) ratio = 1.0f;
			float thumb_h = track_h * ratio;
			float min_th  = FLUX_SCROLLBAR_MIN_THUMB;
			if (track_h < min_th) min_th = track_h;
			if (thumb_h < min_th) thumb_h = min_th;
			if (thumb_h > track_h) thumb_h = track_h;
			float max_scroll = content_h - bounds->h;
			float pct        = (max_scroll > 0.0f) ? scroll_y / max_scroll : 0.0f;
			if (pct < 0.0f) pct = 0.0f;
			if (pct > 1.0f) pct = 1.0f;
			float thumb_y = track_y + (track_h - thumb_h) * pct;
			g->v_thumb    = ( FluxRect ) {bx, thumb_y, bar_w, thumb_h};
		}

		if (g->has_h) {
			float bx      = bounds->x;
			float by      = bounds->y + bounds->h - bar_w;
			g->h_bar      = ( FluxRect ) {bx, by, h_bar_w, bar_w};

			float btn_w   = FLUX_SCROLLBAR_BUTTON_SIZE * expanded;
			g->h_lf_btn   = ( FluxRect ) {bx, by, btn_w, bar_w};
			g->h_rg_btn   = ( FluxRect ) {bx + h_bar_w - btn_w, by, btn_w, bar_w};

			float track_x = bx + btn_w;
			float track_w = h_bar_w - 2.0f * btn_w;
			if (track_w < 0.0f) track_w = 0.0f;
			g->h_track  = ( FluxRect ) {track_x, by, track_w, bar_w};

			float ratio = bounds->w / content_w;
			if (ratio > 1.0f) ratio = 1.0f;
			float thumb_w = track_w * ratio;
			float min_tw  = FLUX_SCROLLBAR_MIN_THUMB;
			if (track_w < min_tw) min_tw = track_w;
			if (thumb_w < min_tw) thumb_w = min_tw;
			if (thumb_w > track_w) thumb_w = track_w;
			float max_scroll = content_w - bounds->w;
			float pct        = (max_scroll > 0.0f) ? scroll_x / max_scroll : 0.0f;
			if (pct < 0.0f) pct = 0.0f;
			if (pct > 1.0f) pct = 1.0f;
			float thumb_x = track_x + (track_w - thumb_w) * pct;
			g->h_thumb    = ( FluxRect ) {thumb_x, by, thumb_w, bar_w};
		}
}

#endif /* FLUX_SCROLL_GEOM_H */
