#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_anim.h"

void flux_rec_note_animating(FluxRenderCtx const *rc, bool active) {
	if (active && rc && rc->animations_active) *rc->animations_active = true;
}

FluxVisualState *flux_rec_vstate(FluxRenderCtx const *rc, FluxRenderSnap const *snap, uint32_t state) {
	FluxCacheEntry *ce = rc && rc->cache && snap ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (!ce) return NULL;
	FluxVisualState *vs = &ce->vstate;
	if (!vs->initialized) *vs = (FluxVisualState) {.state = state, .prev = state, .since = -1e9, .initialized = true};
	else if (vs->state != state) {
		vs->prev  = vs->state;
		vs->state = state;
		vs->since = rc->now;
	}
	return vs;
}

FluxRecStateAnim flux_rec_animstate(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state, bool check_target
) {
	FluxRecStateAnim out = {0};
	if (!state) return out;
	if (!rc || !snap) {
		out.hover = state->hovered ? 1.0f : 0.0f;
		out.press = state->pressed ? 1.0f : 0.0f;
		out.focus = state->focused ? 1.0f : 0.0f;
		out.check = check_target ? 1.0f : 0.0f;
		return out;
	}

	FluxCacheEntry *ce = rc->cache ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (!ce) {
		out.hover = state->hovered ? 1.0f : 0.0f;
		out.press = state->pressed ? 1.0f : 0.0f;
		out.focus = state->focused ? 1.0f : 0.0f;
		out.check = check_target ? 1.0f : 0.0f;
		return out;
	}

	FluxTweenChannels channels = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
	FluxTweenTargets  targets  = {state->hovered, state->pressed, state->focused, check_target};
	flux_rec_note_animating(rc, flux_tween_update_states(&channels, targets, rc->now));
	out.hover = ce->hover_anim.current;
	out.press = ce->press_anim.current;
	out.focus = ce->focus_anim.current;
	out.check = ce->check_anim.current;
	return out;
}

FluxColor flux_rec_tweenfill(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxColor target) {
	if (!rc || !snap || !rc->cache) return target;
	FluxCacheEntry *ce = flux_rcache_ensure(rc->cache, snap->id);
	if (!ce) return target;
	FluxColor out = target;
	flux_rec_note_animating(
	  rc, flux_color_tween_update(&ce->color_anim, target, FLUX_ANIM_DURATION_FAST, rc->now, &out)
	);
	return out;
}

bool flux_rec_fillsink(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *r, float radius, FluxColor fill) {
	if (rc && rc->fill_sink) {
		rc->fill_sink->color         = fill;
		rc->fill_sink->corner_radius = radius;
		rc->fill_sink->written       = true;
		return true;
	}
	return flux_rec_fill_rounded(b, r, radius, fill);
}

bool flux_rec_elevation_border(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, float radius, bool is_accent
) {
	if (!b || !bounds) return false;
	FluxColor secondary  = flux_elevation_secondary(rc, is_accent);
	FluxColor primary    = flux_elevation_primary(rc, is_accent);
	bool      flip       = !rc || !rc->is_dark || is_accent;
	float     band_start = 0.0f, band_end = 0.0f;
	flux_elevation_band(bounds, flip, &band_start, &band_end);

	float    half  = FLUX_STROKE_WIDTH * 0.5f;
	FluxRect inset = {bounds->x + half, bounds->y + half, bounds->w - FLUX_STROKE_WIDTH, bounds->h - FLUX_STROKE_WIDTH};
	float    y0    = flip ? band_end : band_start;
	float    y1    = flip ? band_start : band_end;
	XentLinearGrad grad = {
	  0.0f, y0, 0.0f, y1, FLUX_ELEVATION_STOP1, FLUX_ELEVATION_STOP2, flux_rec_xcolor(secondary),
	  flux_rec_xcolor(primary)};
	return xent_dl_strokerrectgrad(b, flux_rec_xrect(&inset), radius, FLUX_STROKE_WIDTH, &grad);
}

bool flux_rec_focus_rect(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, float radius) {
	if (!b || !t || !bounds) return false;
	FluxRect outer = {
	  bounds->x - FLUX_FOCUS_OUTER_PAD, bounds->y - FLUX_FOCUS_OUTER_PAD, bounds->w + FLUX_FOCUS_OUTER_PAD * 2.0f,
	  bounds->h + FLUX_FOCUS_OUTER_PAD * 2.0f};
	float outer_r = radius + FLUX_FOCUS_OUTER_PAD - 1.0f;
	if (outer_r < 0.0f) outer_r = 0.0f;
	if (!flux_rec_stroke_rounded(b, &outer, outer_r, t->focus_stroke_outer, FLUX_FOCUS_OUTER_THICK)) return false;

	FluxRect inner = {
	  bounds->x - FLUX_FOCUS_INNER_PAD, bounds->y - FLUX_FOCUS_INNER_PAD, bounds->w + FLUX_FOCUS_INNER_PAD * 2.0f,
	  bounds->h + FLUX_FOCUS_INNER_PAD * 2.0f};
	float inner_r = radius + FLUX_FOCUS_INNER_PAD;
	return flux_rec_stroke_rounded(b, &inner, inner_r, t->focus_stroke_inner, FLUX_FOCUS_INNER_THICK);
}

bool flux_rec_fillgrad(
  XentDlBuilder *b, FluxRect const *r, float x0, float y0, float x1, float y1, float s0, float s1, FluxColor c0,
  FluxColor c1
) {
	if (!b || !r) return false;
	XentLinearGrad grad = {x0, y0, x1, y1, s0, s1, flux_rec_xcolor(c0), flux_rec_xcolor(c1)};
	return xent_dl_fillrectgrad(b, flux_rec_xrect(r), &grad);
}
