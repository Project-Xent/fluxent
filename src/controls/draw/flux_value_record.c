/**
 * @file flux_value_record.c
 * @brief Value-input recorders (Slider / RatingControl): a value over a range
 * with hover-preview visuals.
 */
#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_slider_data.h"
#include "controls/data/flux_rating_data.h"

#include "render/flux_anim.h"

#include <math.h>

static float slider_clampf(float value, float min_v, float max_v) {
	if (value < min_v) return min_v;
	if (value > max_v) return max_v;
	return value;
}

typedef struct {
	float cy;
	float track_h;
	float track_left;
	float track_w;
	float thumb_x;
	float thumb_outer;
	float pct;
	float range;
} SliderGeom;

typedef struct {
	FluxColor track_bg;
	FluxColor val;
	FluxColor thumb_fill;
	FluxColor thumb_stroke;
	FluxColor inner;
} SliderColors;

/* While the thumb is held it tracks IntermediateValue; Value stays on the snap
 * grid and the thumb glides back on release (WinUI IntermediateValue contract). */
static SliderGeom slider_geom(FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state) {
	float      value = state->pressed ? snap->u.slider.slider_intermediate : snap->u.slider.current_value;
	SliderGeom g;
	g.track_h     = 4.0f;
	g.thumb_outer = 10.0f;
	g.range       = snap->u.slider.max_value - snap->u.slider.min_value;
	g.pct         = g.range > 0.0f ? (value - snap->u.slider.min_value) / g.range : 0.0f;
	g.pct         = slider_clampf(g.pct, 0.0f, 1.0f);
	g.cy          = bounds->y + bounds->h * 0.5f;
	g.track_left  = bounds->x + g.thumb_outer;
	g.track_w     = slider_clampf(bounds->w - g.thumb_outer * 2.0f, 0.0f, bounds->w);
	g.thumb_x     = g.track_left + g.track_w * g.pct;
	return g;
}

static bool slider_draw_tick_run(
  XentDlBuilder *b, SliderGeom const *g, float y, float h, int count, float interval_px, FluxColor color
) {
	for (int j = 0; j < count; j++) {
		FluxRect tick = {g->track_left + ( float ) j * interval_px, y, 1.0f, h};
		if (!flux_rec_fill_rect(b, &tick, color)) return false;
	}
	return true;
}

/* WinUI TickBar: marks every TickFrequency; intervals closer than 20px are thinned. */
static bool slider_draw_ticks(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, SliderGeom const *g,
  FluxControlState const *state
) {
	float frequency = snap->u.slider.slider_tick_frequency;
	if (frequency <= 0.0f || g->range <= 0.0f || g->track_w <= 0.0f) return true;
	FluxTickPlacement placement = ( FluxTickPlacement ) snap->u.slider.slider_tick_placement;
	if (placement == FLUX_TICK_NONE) return true;

	float num_intervals = fmaxf(1.0f, g->range / frequency);
	int   count         = ( int ) floorf(num_intervals);
	float interval_px   = fmaxf(1.0f, g->track_w / num_intervals);
	float min_gap       = 20.0f;
	if (interval_px < min_gap) {
		int ratio    = ( int ) ceilf(min_gap / interval_px);
		interval_px *= ( float ) ratio;
		count       /= ratio;
	}
	count                            += 1;

	FluxThemeColors const *t          = flux_rec_theme(rc);
	FluxColor              outside    = state->enabled ? t->ctrl_strong_fill_default : t->ctrl_strong_fill_disabled;
	float                  track_top  = g->cy - g->track_h * 0.5f;
	float                  bar_h      = 4.0f;
	float                  gap        = 4.0f;
	bool                   ok         = true;
	if (placement == FLUX_TICK_INLINE)
		ok = slider_draw_tick_run(b, g, track_top, g->track_h, count, interval_px, t->ctrl_fill_input_active);
	if (ok && (placement == FLUX_TICK_TOP_LEFT || placement == FLUX_TICK_OUTSIDE))
		ok = slider_draw_tick_run(b, g, track_top - gap - bar_h, bar_h, count, interval_px, outside);
	if (ok && (placement == FLUX_TICK_BOTTOM_RIGHT || placement == FLUX_TICK_OUTSIDE))
		ok = slider_draw_tick_run(b, g, track_top + g->track_h + gap, bar_h, count, interval_px, outside);
	return ok;
}

static SliderColors
slider_colors(FluxThemeColors const *t, FluxControlState const *state, FluxRecStateAnim const *anim) {
	SliderColors c = {
	  .track_bg     = t->ctrl_strong_fill_default,
	  .thumb_fill   = t->ctrl_solid_fill_default,
	  .thumb_stroke = t->ctrl_stroke_default,
	};
	if (!state->enabled) {
		c.track_bg     = t->ctrl_strong_fill_disabled;
		c.val          = t->accent_disabled;
		c.inner        = t->accent_disabled;
		c.thumb_stroke = t->ctrl_strong_stroke_disabled;
		return c;
	}
	c.val = flux_anim_lerp_color(
	  flux_anim_lerp_color(t->accent_default, t->accent_secondary, anim->hover), t->accent_tertiary, anim->press
	);
	c.inner = c.val;
	return c;
}

static bool slider_draw_track(XentDlBuilder *b, SliderGeom const *g, SliderColors const *c) {
	FluxRect track = {g->track_left, g->cy - g->track_h * 0.5f, g->track_w, g->track_h};
	if (!flux_rec_fill_rounded(b, &track, g->track_h * 0.5f, c->track_bg)) return false;
	float val_w = g->track_w * g->pct;
	if (val_w <= 0.0f) return true;
	FluxRect val_r = {g->track_left, g->cy - g->track_h * 0.5f, val_w, g->track_h};
	return flux_rec_fill_rounded(b, &val_r, g->track_h * 0.5f, c->val);
}

static float slider_thumb_scale(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state) {
	/* Touch keeps full-size thumb (pointer_type 1); mouse/pen scale with hover/press. */
	float target_scale = 1.0f;
	if (state->pointer_type != 1)
		target_scale = state->pressed ? FLUX_SLIDER_SCALE_PRESSED
		             : state->hovered ? FLUX_SLIDER_SCALE_HOVER
		                              : FLUX_SLIDER_SCALE_NORMAL;
	if (!rc || !snap || !rc->cache) return target_scale;
	FluxCacheEntry *ce = flux_rcache_ensure(rc->cache, snap->id);
	if (!ce) return target_scale;
	float scale = target_scale;
	flux_rec_note_animating(
	  rc, flux_tween_update(&ce->slider_anim, target_scale, FLUX_ANIM_DURATION_SLIDER, rc->now, &scale)
	);
	return scale;
}

static bool slider_draw_thumb(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state,
  SliderGeom const *g, SliderColors const *c
) {
	float outer_d = g->thumb_outer * 2.0f;
	if (!flux_rec_fill_circle(b, g->thumb_x, g->cy, outer_d, c->thumb_fill)) return false;
	if (!flux_rec_stroke_circle(b, g->thumb_x, g->cy, outer_d, c->thumb_stroke, 1.0f)) return false;
	float scale   = slider_thumb_scale(rc, snap, state);
	float inner_d = 6.0f * scale * 2.0f; /* original: ellipse radii 6*scale */
	return flux_rec_fill_circle(b, g->thumb_x, g->cy, inner_d, c->inner);
}

bool flux_rec_build_slider(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxRecStateAnim anim   = flux_rec_animstate(rc, snap, state, false);
	SliderGeom       geom   = slider_geom(snap, bounds, state);
	SliderColors     colors = slider_colors(flux_rec_theme(rc), state, &anim);

	bool             ok     = slider_draw_track(b, &geom, &colors);
	if (ok) ok = slider_draw_ticks(b, rc, snap, &geom, state);
	if (ok) ok = slider_draw_thumb(b, rc, snap, state, &geom, &colors);
	if (ok && state->focused && state->enabled) {
		FluxRect thumb = {
		  geom.thumb_x - geom.thumb_outer, geom.cy - geom.thumb_outer, geom.thumb_outer * 2.0f,
		  geom.thumb_outer * 2.0f};
		ok = flux_rec_focus_rect(b, &thumb, geom.thumb_outer);
	}
	return ok;
}

static bool rating_star_fill(
  XentDlBuilder *b, FluxRect const *box, double fill_v, int i, wchar_t const *set, float size, FluxColor fill
) {
	if (( double ) i >= fill_v) return true;
	if (( double ) i + 1.0 <= fill_v) return flux_rec_glyph_wc(b, box, set, size, fill);
	float    frac = ( float ) (fill_v - floor(fill_v));
	FluxRect cell = {box->x, box->y, box->w * frac, box->h};
	if (!xent_dl_pushclip_rect(b, flux_rec_xrect(&cell))) return false;
	bool ok = flux_rec_glyph_wc(b, box, set, size, fill);
	return xent_dl_popclip(b) && ok;
}

static double rating_preview_value(FluxRenderSnap const *snap, FluxControlState const *state, float strip_w) {
	if (!state->enabled || snap->u.rating.is_read_only) return -1.0;
	if (state->pressed) return snap->u.rating.hover_value;
	if (!state->hovered || snap->hover_local_x < 0.0f || snap->hover_local_x > strip_w) return -1.0;
	double v = ceil(( double ) snap->hover_local_x / strip_w * snap->u.rating.max_rating);
	return v > snap->u.rating.max_rating ? ( double ) snap->u.rating.max_rating : v;
}

static FluxColor rating_fill_color(FluxThemeColors const *t, FluxControlState const *state, bool hovering, bool set) {
	if (!state->enabled) return t->text_disabled;
	if (hovering && !set) return t->ctrl_alt_fill_tertiary;
	if (set || hovering) return t->accent_default;
	return t->text_primary;
}

static bool rating_draw_stars(
  XentDlBuilder *b, FluxRect const *bounds, FluxRenderSnap const *snap, FluxThemeColors const *t,
  FluxControlState const *state
) {
	int       max       = snap->u.rating.max_rating;
	float     size      = snap->u.rating.star_size;
	float     gap       = snap->u.rating.item_spacing;
	float     y         = bounds->y + (bounds->h - size) * 0.5f;
	bool      has_value = snap->u.rating.value >= 0.0;
	float     strip_w   = ( float ) max * size + ( float ) (max - 1) * gap;
	double    hover_v   = rating_preview_value(snap, state, strip_w);
	bool      hovering  = hover_v >= 0.0;
	double    fill_v    = hovering                                ? hover_v
	                    : has_value                               ? snap->u.rating.value
	                    : snap->u.rating.placeholder_value >= 0.0 ? snap->u.rating.placeholder_value
	                                                              : 0.0;
	FluxColor fill      = rating_fill_color(t, state, hovering, has_value);
	wchar_t   unset [2] = {( wchar_t ) snap->u.rating.unset_glyph, 0};
	wchar_t   set [2]   = {( wchar_t ) snap->u.rating.set_glyph, 0};

	for (int i = 0; i < max; i++) {
		FluxRect box = {bounds->x + ( float ) i * (size + gap), y, size, size};
		if (!flux_rec_glyph_wc(b, &box, unset, size, t->text_secondary)) return false;
		if (!rating_star_fill(b, &box, fill_v, i, set, size, fill)) return false;
	}
	return true;
}

static bool
rating_draw_caption(XentDlBuilder *b, FluxRect const *bounds, FluxRenderSnap const *snap, FluxThemeColors const *t) {
	if (!snap->u.rating.caption) return true;
	int      max     = snap->u.rating.max_rating;
	float    size    = snap->u.rating.star_size;
	float    gap     = snap->u.rating.item_spacing;
	float    strip_w = ( float ) max * size + ( float ) (max - 1) * gap;
	float    cx      = bounds->x + strip_w + FLUX_RATING_CAPTION_GAP;
	FluxRect cr      = {cx, bounds->y, bounds->w - (cx - bounds->x), bounds->h};
	return flux_rec_text(b, &cr, snap->u.rating.caption, 12.0f, 400u, t->text_secondary);
}

bool flux_rec_build_rating(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t    = flux_rec_theme(rc);
	int                    max  = snap->u.rating.max_rating;
	float                  size = snap->u.rating.star_size;
	if (max < 1 || size <= 0.0f) return true;
	if (!rating_draw_stars(b, bounds, snap, t, state)) return false;
	return rating_draw_caption(b, bounds, snap, t);
}
