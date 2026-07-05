/**
 * @file flux_rating.c
 * @brief RatingControl renderer: an outline "unset" background layer under a
 * "set" foreground layer clipped to the current/hover value (partial fills for
 * fractional values). Preview uses a distinct brush (accent when a value is
 * already set, a low-contrast ghost when unset). Caption trails at the right.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#include "fluxent/controls/flux_rating_data.h"

#include <math.h>

/* Plain-hover preview follows the pointer (OnPointerMovedOverBackgroundStackPanel:
 * ceil(mousePercentage * max) whenever the pointer is over the strip, no press
 * needed) — derived live so the fill keeps tracking after a click. Only a
 * captured drag uses the behavior's scrub value (it clamps past the edges);
 * the caption zone right of the strip previews nothing. */
static double rating_preview_value(FluxRenderSnapshot const *snap, FluxControlState const *state, float strip_w) {
	if (!state->enabled || snap->u.rating.is_read_only) return -1.0;
	if (state->pressed) return snap->u.rating.hover_value;
	if (!state->hovered || snap->hover_local_x < 0.0f || snap->hover_local_x > strip_w) return -1.0;
	double v = ceil(( double ) snap->hover_local_x / strip_w * snap->u.rating.max_rating);
	return v > snap->u.rating.max_rating ? ( double ) snap->u.rating.max_rating : v;
}

/* Foreground fill brush by state (UpdateRatingItemsAppearance). */
static FluxColor rating_fill_color(FluxThemeColors const *t, FluxControlState const *state, bool hovering, bool set) {
	if (!state->enabled) return t->text_disabled;
	if (hovering && !set) return t->ctrl_alt_fill_tertiary; /* ghost preview */
	if (set || hovering) return t->accent_default;
	return t->text_primary; /* placeholder */
}

/* The fractional star clips its own cell to size * frac (WinUI's per-item
 * RectangleGeometry clip) so full neighbours are never cut. */
static void rating_draw_partial_star(
  FluxRenderContext const *rc, FluxRect const *box, wchar_t const *glyph, FluxColor color, float frac
) {
	FluxRect    cell    = {box->x, box->y, box->w * frac, box->h};
	D2D1_RECT_F d2dclip = flux_d2d_rect(&cell);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &d2dclip, D2D1_ANTIALIAS_MODE_ALIASED);
	flux_button_draw_chevron(rc, box, glyph, box->w, color);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void rating_draw_caption(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, float strip_w,
  FluxThemeColors const *t
) {
	if (!snap->u.rating.caption || !rc->text) return;
	float    cx = bounds->x + strip_w + FLUX_RATING_CAPTION_GAP;
	FluxRect cr = {cx, bounds->y, bounds->w - (cx - bounds->x), bounds->h};
	if (cr.w <= 0.0f) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size  = 12.0f;
	ts.text_align = FLUX_TEXT_LEFT;
	ts.vert_align = FLUX_TEXT_VCENTER;
	ts.color      = t->text_secondary;
	flux_text_draw(rc->text, FLUX_RT(rc), snap->u.rating.caption, &cr, &ts);
}

void flux_draw_rating(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t    = rc->theme ? rc->theme : flux_theme_default_colors();
	int                    max  = snap->u.rating.max_rating;
	float                  size = snap->u.rating.star_size;
	float                  gap  = snap->u.rating.item_spacing;
	if (max < 1 || size <= 0.0f) return;

	bool   has_value = snap->u.rating.value >= 0.0;
	float  strip_w   = ( float ) max * size + ( float ) (max - 1) * gap;
	double hover_v   = rating_preview_value(snap, state, strip_w);
	bool   hovering  = hover_v >= 0.0;
	double fill_v    = hovering ? hover_v
	     : has_value            ? snap->u.rating.value
	     : snap->u.rating.placeholder_value >= 0.0 ? snap->u.rating.placeholder_value
	                                               : 0.0;

	FluxColor fill = rating_fill_color(t, state, hovering, has_value);

	/* Star cell == the advance box: WinUI lays items out at ActualRatingFontSize
	 * (16dip) and its resting render scale puts the glyph ink at that same size,
	 * leaving the full 8dip ItemSpacing visible between stars. The background
	 * outline layer draws whole; the fill layer draws full stars unclipped,
	 * clips the fractional star to its cell, and skips empty stars. */
	float   y         = bounds->y + (bounds->h - size) * 0.5f;
	wchar_t unset [2] = {( wchar_t ) snap->u.rating.unset_glyph, 0};
	wchar_t set [2]   = {( wchar_t ) snap->u.rating.set_glyph, 0};

	for (int i = 0; i < max; i++) {
		FluxRect box = {bounds->x + ( float ) i * (size + gap), y, size, size};
		flux_button_draw_chevron(rc, &box, unset, size, t->text_secondary);
		if (( double ) i >= fill_v) continue;
		if (( double ) i + 1.0 <= fill_v) flux_button_draw_chevron(rc, &box, set, size, fill);
		else rating_draw_partial_star(rc, &box, set, fill, ( float ) (fill_v - floor(fill_v)));
	}

	rating_draw_caption(rc, snap, bounds, strip_w, t);
}
