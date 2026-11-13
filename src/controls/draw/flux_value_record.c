#include "controls/draw/flux_control_record_internal.h"
#include "controls/behavior/flux_color_picker.h"

#include "controls/data/flux_slider_data.h"
#include "controls/data/flux_rating_data.h"

#include "render/flux_anim.h"
#include "render/flux_slider_geom.h"

#include <math.h>

static float slider_clampf(float value, float min_v, float max_v) {
	if (value < min_v) return min_v;
	if (value > max_v) return max_v;
	return value;
}

typedef struct {
	FluxSliderLine line;
	float          track_h;
	float          thumb_outer;
	float          range;
	int            thumbs;
	float          pct [2];
	float          fill_lo;
	float          fill_hi;
} SliderGeom;

typedef struct {
	FluxColor track_bg;
	FluxColor val;
	FluxColor thumb_fill;
	FluxColor thumb_stroke;
	FluxColor inner;
} SliderColors;

static float slider_thumb_value(FluxSliderSnapshot const *s, FluxControlState const *state, int i) {
	if (!s->is_range) return state->pressed ? s->slider_intermediate : s->current_value;
	bool held = state->pressed && s->thumb_held && s->active_thumb == i;
	return held ? s->range_live [i] : s->range_value [i];
}

static SliderGeom slider_geom(FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state) {
	FluxSliderSnapshot const *s = &snap->u.slider;
	SliderGeom                g = {.track_h = 4.0f, .thumb_outer = FLUX_SLIDER_THUMB_OUTER};
	g.line                      = flux_slider_line(bounds->x, bounds->y, bounds->w, bounds->h, s->vertical);
	g.range                     = s->max_value - s->min_value;
	g.thumbs                    = s->is_range ? 2 : 1;
	for (int i = 0; i < g.thumbs; i++) {
		float v   = slider_thumb_value(s, state, i);
		g.pct [i] = g.range > 0.0f ? slider_clampf((v - s->min_value) / g.range, 0.0f, 1.0f) : 0.0f;
	}
	g.fill_lo = g.thumbs == 2 ? fminf(g.pct [0], g.pct [1]) : 0.0f;
	g.fill_hi = g.thumbs == 2 ? fmaxf(g.pct [0], g.pct [1]) : g.pct [0];
	return g;
}

static FluxRect slider_span(SliderGeom const *g, float a0, float a1, float cross, float thickness) {
	float lo  = fminf(a0, a1);
	float len = fabsf(a1 - a0);
	if (g->line.vertical) return (FluxRect) {cross, lo, thickness, len};
	return (FluxRect) {lo, cross, len, thickness};
}

static float slider_track_edge(SliderGeom const *g) { return g->line.cross - g->track_h * 0.5f; }

static bool  slider_draw_tick_run(
  XentDlBuilder *b, SliderGeom const *g, float cross, float h, int count, float interval_px, FluxColor color
) {
	for (int j = 0; j < count; j++) {
		float    offset = ( float ) j * interval_px;
		float    a = g->line.vertical ? g->line.start + g->line.length - offset - 0.5f : g->line.start + offset - 0.5f;
		FluxRect tick = slider_span(g, a, a + 1.0f, cross, h);
		if (!flux_rec_fill_rect(b, &tick, color)) return false;
	}
	return true;
}

static bool slider_draw_ticks(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, SliderGeom const *g,
  FluxControlState const *state
) {
	float frequency = snap->u.slider.slider_tick_frequency;
	if (frequency <= 0.0f || g->range <= 0.0f || g->line.length <= 0.0f) return true;
	FluxTickPlacement placement = ( FluxTickPlacement ) snap->u.slider.slider_tick_placement;
	if (placement == FLUX_TICK_NONE) return true;

	float num_intervals = fmaxf(1.0f, g->range / frequency);
	int   count         = ( int ) floorf(num_intervals);
	float interval_px   = fmaxf(1.0f, g->line.length / num_intervals);
	float min_gap       = 20.0f;
	if (interval_px < min_gap) {
		int ratio    = ( int ) ceilf(min_gap / interval_px);
		interval_px *= ( float ) ratio;
		count       /= ratio;
	}
	count                            += 1;

	FluxThemeColors const *t          = flux_rec_theme(rc);
	FluxColor              outside    = state->enabled ? t->ctrl_strong_fill_default : t->ctrl_strong_fill_disabled;
	float                  near_edge  = slider_track_edge(g);
	float                  bar_h      = 4.0f;
	float                  gap        = 4.0f;
	bool                   ok         = true;
	if (placement == FLUX_TICK_INLINE)
		ok = slider_draw_tick_run(b, g, near_edge, g->track_h, count, interval_px, t->ctrl_fill_input_active);
	if (ok && (placement == FLUX_TICK_TOP_LEFT || placement == FLUX_TICK_OUTSIDE))
		ok = slider_draw_tick_run(b, g, near_edge - gap - bar_h, bar_h, count, interval_px, outside);
	if (ok && (placement == FLUX_TICK_BOTTOM_RIGHT || placement == FLUX_TICK_OUTSIDE))
		ok = slider_draw_tick_run(b, g, near_edge + g->track_h + gap, bar_h, count, interval_px, outside);
	return ok;
}

static SliderColors slider_colors(FluxThemeColors const *t, FluxControlState const *state) {
	SliderColors c = {
	  .track_bg     = t->ctrl_strong_fill_default,
	  .thumb_fill   = t->ctrl_solid_fill_default,
	  .thumb_stroke = t->ctrl_stroke_default,
	  .val          = state->pressed ? t->accent_tertiary
	                : state->hovered ? t->accent_secondary
	                                 : t->accent_default,
	};
	if (!state->enabled) {
		c.track_bg = t->ctrl_strong_fill_disabled;
		c.val      = t->accent_disabled;
	}
	c.inner = c.val;
	return c;
}

static bool slider_draw_track(XentDlBuilder *b, SliderGeom const *g, SliderColors const *c) {
	float    edge  = slider_track_edge(g);
	float    lo    = g->line.start - FLUX_SLIDER_THUMB_TRAVEL;
	float    hi    = g->line.start + g->line.length + FLUX_SLIDER_THUMB_TRAVEL;
	FluxRect track = slider_span(g, lo, hi, edge, g->track_h);
	if (!flux_rec_fill_rounded(b, &track, g->track_h * 0.5f, c->track_bg)) return false;
	if (g->fill_hi <= g->fill_lo && g->thumbs == 2) return true;
	float    a0   = g->thumbs == 2 ? flux_slider_along(&g->line, g->fill_lo) : (g->line.vertical ? hi : lo);
	float    a1   = flux_slider_along(&g->line, g->fill_hi);
	FluxRect fill = slider_span(g, a0, a1, edge, g->track_h);
	return flux_rec_fill_rounded(b, &fill, g->track_h * 0.5f, c->val);
}

static int slider_hot_thumb(
  FluxRenderSnap const *snap, FluxRect const *bounds, SliderGeom const *g, FluxControlState const *state
) {
	FluxSliderSnapshot const *s = &snap->u.slider;
	if (!s->is_range) return 0;
	bool hovering = state->hovered && !state->pressed && snap->hover_local_x >= 0.0f;
	if (!hovering) return s->active_thumb;
	float pos = g->line.vertical ? bounds->y + snap->hover_local_y : bounds->x + snap->hover_local_x;
	float d0  = fabsf(pos - flux_slider_along(&g->line, g->pct [0]));
	float d1  = fabsf(pos - flux_slider_along(&g->line, g->pct [1]));
	return d1 < d0 ? 1 : 0;
}

static float ease_slider_scale(float t) { return flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f); }

static float slider_target_scale(FluxControlState const *state) {
	if (!state->enabled) return FLUX_SLIDER_SCALE_HOVER;
	if (state->pressed) return FLUX_SLIDER_SCALE_PRESSED;
	if (state->hovered && state->pointer_type != 1) return FLUX_SLIDER_SCALE_HOVER;
	return FLUX_SLIDER_SCALE_NORMAL;
}

static float slider_thumb_scale(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state) {
	float target = slider_target_scale(state);
	if (!rc || !snap || !rc->cache) return target;
	FluxCacheEntry *ce = flux_rcache_ensure(rc->cache, snap->id);
	if (!ce) return target;
	float  scale           = target;
	double duration        = target == FLUX_SLIDER_SCALE_NORMAL ? 0.167 : 0.25;
	ce->slider_anim.easing = ease_slider_scale;
	flux_rec_note_animating(rc, flux_tween_update(&ce->slider_anim, target, duration, rc->now, &scale));
	return scale;
}

static FluxPoint slider_thumb_center(SliderGeom const *g, int i) {
	float along = flux_slider_along(&g->line, g->pct [i]);
	if (g->line.vertical) return (FluxPoint) {g->line.cross, along};
	return (FluxPoint) {along, g->line.cross};
}

static bool slider_draw_thumb(
  XentDlBuilder *b, FluxRenderCtx const *rc, SliderGeom const *g, SliderColors const *c, int i, float scale
) {
	FluxPoint at    = slider_thumb_center(g, i);
	float     d     = g->thumb_outer * 2.0f;
	FluxRect  outer = {at.x - g->thumb_outer, at.y - g->thumb_outer, d, d};
	if (!flux_rec_fill_rounded(b, &outer, 10.0f, c->thumb_fill)) return false;
	if (!flux_rec_elevation_border(b, rc, &outer, 10.0f, false)) return false;
	return flux_rec_fill_circle(b, at.x, at.y, FLUX_SLIDER_INNER_THUMB * scale, c->inner);
}

bool flux_rec_build_slider(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (snap->u.slider.color_picker) return flux_colorpicker_slider_record(b, rc, snap, bounds, state);
	SliderGeom   geom   = slider_geom(snap, bounds, state);
	SliderColors colors = slider_colors(flux_rec_theme(rc), state);

	bool         ok     = slider_draw_track(b, &geom, &colors);
	if (ok) ok = slider_draw_ticks(b, rc, snap, &geom, state);

	int   hot        = slider_hot_thumb(snap, bounds, &geom, state);
	float hot_scale  = slider_thumb_scale(rc, snap, state);
	float idle_scale = state->enabled ? FLUX_SLIDER_SCALE_NORMAL : FLUX_SLIDER_SCALE_HOVER;
	if (ok && geom.thumbs == 2) ok = slider_draw_thumb(b, rc, &geom, &colors, 1 - hot, idle_scale);
	if (ok) ok = slider_draw_thumb(b, rc, &geom, &colors, hot, hot_scale);
	if (ok && state->focused && state->enabled) {
		FluxRect fr = {bounds->x - 4.0f, bounds->y + 3.0f, bounds->w + 8.0f, bounds->h - 6.0f};
		ok          = flux_rec_focus_rect(b, flux_rec_theme(rc), &fr, FLUX_CORNER_RADIUS);
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
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(&cell))) return false;
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
