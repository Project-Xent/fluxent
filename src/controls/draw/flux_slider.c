#include "render/flux_render_internal.h"
#include "render/flux_fluent.h"
#include <math.h>

static float slider_clampf(float value, float min, float max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

typedef struct SliderAnimState {
	FluxCacheEntry *cache;
	float           hover_t;
	float           press_t;
} SliderAnimState;

typedef struct SliderGeom {
	float cy;
	float track_h;
	float track_left;
	float track_w;
	float thumb_x;
	float thumb_outer;
	float pct;
	float range;
} SliderGeom;

typedef struct SliderColors {
	FluxColor track_bg;
	FluxColor val;
	FluxColor thumb_fill;
	FluxColor thumb_stroke;
	FluxColor inner;
} SliderColors;

static void slider_update_animation(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, SliderAnimState *anim
) {
	FluxCacheEntry *ce      = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float           hover_t = state->hovered ? 1.0f : 0.0f;
	float           press_t = state->pressed ? 1.0f : 0.0f;

	if (ce) {
		FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
		FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, false};
		bool              animating = flux_tween_update_states(&channels, targets, rc->now);
		if (animating && rc->animations_active) *rc->animations_active = true;
		hover_t = ce->hover_anim.current;
		press_t = ce->press_anim.current;
	}

	anim->cache   = ce;
	anim->hover_t = hover_t;
	anim->press_t = press_t;
}

/* While the thumb is held it tracks IntermediateValue (the raw pointer
 * position); Value stays on the snap grid and the thumb glides back to it on
 * release (WinUI Slider's IntermediateValue contract). */
static SliderGeom slider_geom(FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state) {
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

/* One TickBar run: 1px-wide marks every interval_px starting at the thumb
 * midpoint rest position (TickBar_Partial.cpp ArrangeOverride). */
static void slider_draw_tick_run(
  FluxRenderContext const *rc, SliderGeom const *g, float y, float h, int count, float interval_px, FluxColor color
) {
	for (int j = 0; j < count; j++)
		flux_fill_rect(rc, &(FluxRect) {g->track_left + ( float ) j * interval_px, y, 1.0f, h}, color);
}

/* WinUI TickBar: marks every TickFrequency across the thumb travel range, one
 * per full interval plus the start; intervals closer than MIN_TICKMARK_GAP
 * (20px) are thinned to the smallest multiple that clears it. Outside bars are
 * 4px tall with a 4px gap from the track; Inline draws over the track at track
 * height with the input-active fill (Slider_themeresources.xaml). */
static void slider_draw_ticks(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, SliderGeom const *g, FluxControlState const *state
) {
	float frequency = snap->u.slider.slider_tick_frequency;
	if (frequency <= 0.0f || g->range <= 0.0f || g->track_w <= 0.0f) return;
	FluxTickPlacement placement = ( FluxTickPlacement ) snap->u.slider.slider_tick_placement;
	if (placement == FLUX_TICK_NONE) return;

	float num_intervals = fmaxf(1.0f, g->range / frequency);
	int   count         = ( int ) floorf(num_intervals);
	float interval_px   = fmaxf(1.0f, g->track_w / num_intervals);
	float min_gap       = 20.0f;
	if (interval_px < min_gap) {
		int ratio    = ( int ) ceilf(min_gap / interval_px);
		interval_px *= ( float ) ratio;
		count       /= ratio;
	}
	count                            += 1; /* one mark at the start of the run */

	FluxThemeColors const *t          = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxColor              outside    = state->enabled ? t->ctrl_strong_fill_default : t->ctrl_strong_fill_disabled;

	float                  track_top  = g->cy - g->track_h * 0.5f;
	float                  bar_h      = 4.0f;
	float                  gap        = 4.0f;
	if (placement == FLUX_TICK_INLINE)
		slider_draw_tick_run(rc, g, track_top, g->track_h, count, interval_px, t->ctrl_fill_input_active);
	if (placement == FLUX_TICK_TOP_LEFT || placement == FLUX_TICK_OUTSIDE)
		slider_draw_tick_run(rc, g, track_top - gap - bar_h, bar_h, count, interval_px, outside);
	if (placement == FLUX_TICK_BOTTOM_RIGHT || placement == FLUX_TICK_OUTSIDE)
		slider_draw_tick_run(rc, g, track_top + g->track_h + gap, bar_h, count, interval_px, outside);
}

static SliderColors slider_base_colors(FluxThemeColors const *t) {
	return (SliderColors) {
	  .track_bg     = t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 0x9c),
	  .thumb_fill   = t ? t->ctrl_solid_fill_default : flux_color_rgb(255, 255, 255),
	  .thumb_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f),
	};
}

static SliderColors slider_disabled_colors(FluxThemeColors const *t) {
	SliderColors c = slider_base_colors(t);
	c.track_bg     = t ? t->ctrl_strong_fill_disabled : flux_color_rgba(0, 0, 0, 0x37);
	c.val          = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
	c.inner        = t ? t->accent_disabled : flux_color_rgba(0, 120, 212, 100);
	c.thumb_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
	return c;
}

static FluxColor slider_active_value_color(FluxThemeColors const *t, SliderAnimState const *anim) {
	FluxColor acc_n = t ? t->accent_default : flux_color_rgb(0, 120, 212);
	FluxColor acc_h = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xe6);
	FluxColor acc_p = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xcc);
	return flux_anim_lerp_color(flux_anim_lerp_color(acc_n, acc_h, anim->hover_t), acc_p, anim->press_t);
}

static SliderColors
slider_colors(FluxThemeColors const *t, FluxControlState const *state, SliderAnimState const *anim) {
	if (!state->enabled) return slider_disabled_colors(t);

	SliderColors c = slider_base_colors(t);
	c.val          = slider_active_value_color(t, anim);
	c.inner        = c.val;
	return c;
}

/* Template z-order: track rect, then the value fill, then the tick bars, then
 * the thumb (Slider_themeresources.xaml HorizontalTemplate). */
static void slider_draw_track(FluxRenderContext const *rc, SliderGeom const *g, SliderColors const *c) {
	FluxRect track = {g->track_left, g->cy - g->track_h * 0.5f, g->track_w, g->track_h};
	flux_fill_rounded_rect(rc, &track, g->track_h * 0.5f, c->track_bg);

	float val_w = g->track_w * g->pct;
	if (val_w <= 0.0f) return;

	FluxRect val_r = {g->track_left, g->cy - g->track_h * 0.5f, val_w, g->track_h};
	flux_fill_rounded_rect(rc, &val_r, g->track_h * 0.5f, c->val);
}

static float
slider_thumb_scale(FluxRenderContext const *rc, FluxControlState const *state, SliderAnimState const *anim) {
	float target_scale = 1.0f;
	if (state->pointer_type != 1) target_scale = state->pressed ? 0.71f : state->hovered ? 1.0f : 0.86f;
	if (!anim->cache) return target_scale;

	float scale = 0.0f;
	bool  active
	  = flux_tween_update(&anim->cache->slider_anim, target_scale, FLUX_ANIM_DURATION_SLIDER, rc->now, &scale);
	if (active && rc->animations_active) *rc->animations_active = true;
	return scale;
}

static void slider_draw_thumb(
  FluxRenderContext const *rc, FluxControlState const *state, SliderGeom const *g, SliderColors const *c,
  SliderAnimState const *anim
) {
	flux_fill_ellipse(rc, &(FluxEllipseSpec) {g->thumb_x, g->cy, g->thumb_outer, g->thumb_outer}, c->thumb_fill);
	flux_stroke_ellipse(
	  rc, &(FluxEllipseSpec) {g->thumb_x, g->cy, g->thumb_outer, g->thumb_outer}, c->thumb_stroke, 1.0f
	);

	float scale = slider_thumb_scale(rc, state, anim);
	flux_fill_ellipse(rc, &(FluxEllipseSpec) {g->thumb_x, g->cy, 6.0f * scale, 6.0f * scale}, c->inner);
}

void flux_draw_slider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	SliderAnimState anim = {0};
	slider_update_animation(rc, snap, state, &anim);

	SliderGeom   geom   = slider_geom(snap, bounds, state);
	SliderColors colors = slider_colors(rc->theme, state, &anim);
	slider_draw_track(rc, &geom, &colors);
	slider_draw_ticks(rc, snap, &geom, state);
	slider_draw_thumb(rc, state, &geom, &colors, &anim);

	if (state->focused && state->enabled) {
		FluxRect thumb = {
		  geom.thumb_x - geom.thumb_outer, geom.cy - geom.thumb_outer, geom.thumb_outer * 2.0f,
		  geom.thumb_outer * 2.0f};
		flux_draw_focus_rect(rc, &thumb, geom.thumb_outer);
	}
}
