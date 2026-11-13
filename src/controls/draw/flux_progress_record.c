#include "controls/draw/flux_control_record_internal.h"

#include "render/flux_anim.h"

#include <math.h>
#include <stdio.h>

#include "theme/flux_theme_anim.h"

#include <vsanimation.h>

static float progress_unit(float value) {
	if (value <= 0.0f) return 0.0f;
	if (value >= 1.0f) return 1.0f;
	return value;
}

enum
{
	PB_DETERMINATE,
	PB_PAUSED,
	PB_ERROR,
	PB_INDETERMINATE,
	PB_IND_PAUSED,
	PB_IND_ERROR,
};

typedef struct ProgressCtx {
	XentDlBuilder         *b;
	FluxRenderCtx const   *rc;
	FluxThemeColors const *t;
	FluxRect const        *bounds;
	FluxVisualState const *vs;
	double                 age;
} ProgressCtx;

static void progress_keep_animating(ProgressCtx const *c, bool active) {
	if (active) flux_rec_note_animating(c->rc, true);
}

static FluxRect progress_bar(ProgressCtx const *c, float x, float w) {
	return (FluxRect) {c->bounds->x + x, c->bounds->y + (c->bounds->h - 3.0f) * 0.5f, w, 3.0f};
}

static bool progress_clipped(ProgressCtx const *c, FluxRect const *bars, int n, FluxColor color) {
	if (!xent_dl_pushcliprect(c->b, flux_rec_xrect(c->bounds))) return false;
	bool ok = true;
	for (int i = 0; ok && i < n; ++i) ok = flux_rec_fill_rounded(c->b, &bars [i], 1.5f, color);
	return xent_dl_popclip(c->b) && ok;
}

static bool progress_loop(ProgressCtx const *c, double phase) {
	float    w        = c->bounds->w;
	float    t1       = flux_cubic_bezier(progress_unit(( float ) (phase / 1.5)), 0.4f, 0.0f, 0.6f, 1.0f);
	float    t2       = flux_cubic_bezier(progress_unit(( float ) ((phase - 0.75) / 1.25)), 0.4f, 0.0f, 0.6f, 1.0f);
	FluxRect bars [2] = {
	  progress_bar(c, w * 0.4f * (-1.0f + 4.0f * t1), w * 0.4f),
	  progress_bar(c, w * 0.6f * (-1.5f + 3.16f * t2), w * 0.6f),
	};
	progress_keep_animating(c, true);
	return progress_clipped(c, bars, 2, c->t->accent_default);
}

static bool progress_indeterminate(ProgressCtx const *c) {
	bool from_hold = c->vs && (c->vs->prev == PB_IND_PAUSED || c->vs->prev == PB_IND_ERROR);
	if (!from_hold || c->age >= 0.5) {
		double since = c->vs ? c->age - (from_hold ? 0.5 : 0.0) : c->rc->now;
		return progress_loop(c, fmod(since > 0.0 ? since : 0.0, 2.0));
	}
	float    u   = flux_cubic_bezier(progress_unit(( float ) (c->age / 0.333)), 1.0f, 0.0f, 1.0f, 1.0f);
	FluxRect bar = progress_bar(c, c->bounds->w * 1.2f * u, c->bounds->w * 0.6f);
	progress_keep_animating(c, true);
	return progress_clipped(c, &bar, 1, c->t->accent_default);
}

static FluxColor progress_state_tint(ProgressCtx const *c, uint32_t state) {
	bool dark = c->rc && c->rc->is_dark;
	if (state == PB_PAUSED || state == PB_IND_PAUSED)
		return dark ? flux_color_rgba(0xfc, 0xe1, 0x00, 0xff) : flux_color_rgba(0x9d, 0x5d, 0x00, 0xff);
	if (state == PB_ERROR || state == PB_IND_ERROR)
		return dark ? flux_color_rgba(0xff, 0x99, 0xa4, 0xff) : flux_color_rgba(0xc4, 0x2b, 0x1c, 0xff);
	return c->t->accent_default;
}

static float     ease_linear(float t) { return t; }

static FluxColor progress_color(ProgressCtx const *c, FluxRenderSnap const *snap, uint32_t state) {
	FluxColor       target = progress_state_tint(c, state);
	FluxCacheEntry *ce     = c->rc && c->rc->cache ? flux_rcache_ensure(c->rc->cache, snap->id) : NULL;
	if (!ce) return target;
	FluxColor out;
	ce->color_anim.easing = ease_linear;
	progress_keep_animating(c, flux_color_tween_update(&ce->color_anim, target, 0.167, c->rc->now, &out));
	return out;
}

static float progress_hold_offset(double age) {
	if (age < 0.167) return 0.996f * flux_cubic_bezier(( float ) (age / 0.167), 1.0f, 1.0f, 0.0f, 1.0f);
	if (age >= 0.75) return 0.0f;
	float u = ( float ) ((age - 0.167) / (0.75 - 0.167));
	return -0.9f * (1.0f - flux_cubic_bezier(u, 0.0f, 0.0f, 0.0f, 1.0f));
}

static bool progress_hold(ProgressCtx const *c, FluxColor color) {
	double   age = c->vs ? c->age : 1.0;
	FluxRect bar = progress_bar(c, c->bounds->w * progress_hold_offset(age), c->bounds->w);
	progress_keep_animating(c, age < 0.75);
	return progress_clipped(c, &bar, 1, color);
}

static float progress_reposition(ProgressCtx const *c, FluxRenderSnap const *snap, float width, uint32_t state) {
	FluxCacheEntry *ce = c->rc && c->rc->cache ? flux_rcache_ensure(c->rc->cache, snap->id) : NULL;
	if (!ce) return 0.0f;
	FluxTween *tw = &ce->progress_anim;
	if (!tw->initialized) *tw = (FluxTween) {.target = width, .initialized = true};
	if (width != tw->target) {
		tw->start_val  = state == PB_PAUSED ? 0.0f : tw->target - width;
		tw->start_time = c->rc->now;
		tw->target     = width;
	}
	FluxThemeAnim a = flux_themeanim_get(
	  TAS_REPOSITION, TA_REPOSITION_TARGET, FLUX_THEMEANIM_TRANSLATE,
	  (FluxThemeAnim) {0.0, 0.367, 0.1f, 0.9f, 0.2f, 1.0f, 0, 0}
	);
	double elapsed = c->rc->now - tw->start_time;
	progress_keep_animating(c, tw->start_val != 0.0f && elapsed < flux_themeanim_end(&a));
	return tw->start_val * (1.0f - flux_themeanim_progress(&a, elapsed));
}

static float progress_track_opacity(ProgressCtx const *c) {
	if (!c->vs || c->vs->prev != PB_INDETERMINATE) return 1.0f;
	FluxThemeAnim a = flux_themeanim_get(
	  TAS_FADEIN, TA_FADEIN_SHOWN, FLUX_THEMEANIM_OPACITY,
	  (FluxThemeAnim) {0.0, 0.167, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}
	);
	progress_keep_animating(c, c->age < flux_themeanim_end(&a));
	return a.from + (a.to - a.from) * flux_themeanim_progress(&a, c->age);
}

static FluxColor progress_fade(FluxColor c, float opacity) {
	return flux_color_rgba(
	  (c.rgba >> 24) & 0xff, (c.rgba >> 16) & 0xff, (c.rgba >> 8) & 0xff,
	  ( uint8_t ) (flux_color_af(c) * opacity * 255.0f + 0.5f)
	);
}

static bool progress_determinate(ProgressCtx const *c, FluxRenderSnap const *snap, uint32_t state, FluxColor fill_c) {
	FluxProgressSnapshot const *p     = &snap->u.progress;
	float                       range = p->max_value - p->min_value;
	float                       pct   = range > 0.0f ? progress_unit((p->current_value - p->min_value) / range) : 0.0f;
	float                       width = c->bounds->w * pct;
	float                       shift = progress_reposition(c, snap, width, state);
	float                       cy    = c->bounds->y + c->bounds->h * 0.5f;
	float                       track_h = fminf(c->bounds->h, 1.0f);
	FluxRect                    track   = {c->bounds->x, cy - track_h * 0.5f, c->bounds->w, track_h};
	FluxColor                   tc      = progress_fade(c->t->ctrl_strong_stroke_default, progress_track_opacity(c));
	if (!flux_rec_fill_rounded(c->b, &track, 0.5f, tc)) return false;
	if (width <= 0.0f) return true;
	FluxRect fill = progress_bar(c, shift, width);
	return progress_clipped(c, &fill, 1, fill_c);
}

static FluxRect progress_content_bounds(FluxRect const *bounds, XentInsets padding, float border) {
	return (FluxRect) {
	  bounds->x + padding.left + border,
	  bounds->y + padding.top + border,
	  fmaxf(0.0f, bounds->w - padding.left - padding.right - 2.0f * border),
	  fmaxf(0.0f, bounds->h - padding.top - padding.bottom - 2.0f * border),
	};
}

bool flux_rec_build_progress(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	FluxProgressSnapshot const *p      = &snap->u.progress;
	uint32_t                    state  = (p->indeterminate ? PB_INDETERMINATE : PB_DETERMINATE) + p->state;
	FluxVisualState const      *vs     = flux_rec_vstate(rc, snap, state);
	FluxThemeColors const      *theme  = flux_rec_theme(rc);
	float                       border = fminf(snap->border_width, fminf(bounds->w, bounds->h) * 0.5f);
	if (!flux_rec_stroke_rounded(b, bounds, 1.5f, theme->ctrl_stroke_default, border)) return false;

	FluxRect content = progress_content_bounds(bounds, p->padding, border);
	if (content.w <= 0.0f || content.h <= 0.0f) return true;
	ProgressCtx c    = {b, rc, theme, &content, vs, vs ? rc->now - vs->since : 1e9};
	FluxColor   fill = progress_color(&c, snap, state);
	if (state == PB_INDETERMINATE) return progress_indeterminate(&c);
	if (state > PB_INDETERMINATE) return progress_hold(&c, fill);
	return progress_determinate(&c, snap, state, fill);
}

enum
{
	RING_INACTIVE,
	RING_DETERMINATE,
	RING_INDETERMINATE,
};

static double progress_ring_phase(FluxRenderCtx const *rc, FluxRenderSnap const *snap, uint32_t state) {
	if (!rc) return 0.0;
	FluxVisualState *vs = flux_rec_vstate(rc, snap, state);
	if (!vs) return rc->now;
	if (vs->since < 0.0) vs->since = rc->now;
	return rc->now - vs->since;
}

static float ring_ease(float t) { return flux_cubic_bezier(progress_unit(t), 0.167f, 0.167f, 0.833f, 1.0f - 0.167f); }

typedef struct RingArc {
	float from, to;
	float rotation;
	float radius, stroke;
} RingArc;

static RingArc ring_indeterminate(double phase, float size) {
	float   t    = ( float ) fmod(phase, 2.0) / 2.0f;
	bool    late = t >= 0.5f;
	float   e    = ring_ease((late ? t - 0.5f : t) / 0.5f);
	RingArc a    = {late ? 0.5f * e : 0.0f, late ? 0.5f : 0.5f * e, (late ? 450.0f : 0.0f) + 450.0f * e, 0, 0};
	a.radius     = 35.0f * size / 80.0f;
	a.stroke     = 7.5f * size / 80.0f;
	return a;
}

static float ring_determinate_trim(float p) {
	static float const key [][2] = {
	  {0.0083333f, 0.0001f   },
      {0.25f,      0.25f     },
      {0.5f,       0.5f      },
      {0.75f,      0.75f     },
      {0.9833333f, 0.9666667f},
	  {0.9916667f, 1.0f      }
    };
	if (p < key [0][0]) return 0.0001f;
	for (int i = 1; i < 6; ++i)
		if (p < key [i][0])
			return key [i - 1][1]
			     + (key [i][1] - key [i - 1][1]) * ring_ease((p - key [i - 1][0]) / (key [i][0] - key [i - 1][0]));
	return 1.0f;
}

static float   ease_ring_linear(float t) { return t; }

static RingArc ring_determinate(FluxRenderCtx const *rc, FluxRenderSnap const *snap, float size) {
	FluxProgressSnapshot const *p      = &snap->u.progress;
	float                       range  = p->max_value - p->min_value;
	float                       target = range > 0.0f ? progress_unit((p->current_value - p->min_value) / range) : 0.0f;
	float                       shown  = target;
	FluxCacheEntry             *ce     = rc && rc->cache ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (ce) {
		FluxTween *tw = &ce->progress_anim;
		tw->easing    = ease_ring_linear;
		if (tw->initialized && target < tw->current) tw->current = tw->start_val = tw->target = target;
		double dur = tw->initialized ? fabs(( double ) target - tw->current) * 2.0 : 0.0;
		flux_rec_note_animating(rc, flux_tween_update(tw, target, dur, rc->now, &shown));
	}
	return (RingArc) {0.0f, ring_determinate_trim(shown), 0.0f, 14.16f * size / 32.0f, 2.655f * size / 32.0f};
}

bool flux_rec_progressring(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	if (snap->u.progress.inactive) {
		flux_rec_vstate(rc, snap, RING_INACTIVE);
		return true;
	}
	uint32_t state = snap->u.progress.indeterminate ? RING_INDETERMINATE : RING_DETERMINATE;
	double   phase = progress_ring_phase(rc, snap, state);
	float    size  = bounds->w < bounds->h ? bounds->w : bounds->h;
	RingArc  a     = state == RING_INDETERMINATE ? ring_indeterminate(phase, size) : ring_determinate(rc, snap, size);
	if (state == RING_INDETERMINATE) flux_rec_note_animating(rc, true);
	float sweep = (a.to - a.from) * 360.0f;
	if (sweep <= 0.01f || a.radius < 1.0f) return true;
	if (sweep > 359.99f) sweep = 359.99f;
	float    cx   = bounds->x + bounds->w * 0.5f;
	float    cy   = bounds->y + bounds->h * 0.5f;
	FluxRect ring = {cx - a.radius, cy - a.radius, a.radius * 2.0f, a.radius * 2.0f};
	float    from = a.from * 360.0f + a.rotation - 90.0f;
	return flux_rec_stroke_arc(
	  b, &ring, from, sweep, flux_rec_theme(rc)->accent_default, a.stroke, XENT_STROKE_CAP_ROUND
	);
}

static bool badge_number(XentDlBuilder *b, FluxRect const *bounds, int value, FluxColor accent, FluxColor text) {
	if (value < 1) return true;
	char num [16];
	snprintf(num, sizeof(num), "%d", value);
	float    badge_h = 16.0f;
	float    badge_w = bounds->w > badge_h ? bounds->w : badge_h;
	float    bx      = bounds->x + (bounds->w - badge_w) * 0.5f;
	float    by      = bounds->y + (bounds->h - badge_h) * 0.5f;
	FluxRect badge   = {bx, by, badge_w, badge_h};
	if (!flux_rec_fill_rounded(b, &badge, badge_h * 0.5f, accent)) return false;
	FluxRect      text_box = {badge.x + 4.0f, badge.y, badge.w - 8.0f, badge.h - 2.0f};
	XentTextStyle style    = {.font_size = 11.0f, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, &text_box, num, &style, text);
}

static bool
badge_icon(XentDlBuilder *b, FluxRect const *bounds, char const *icon_name, FluxColor accent, FluxColor text) {
	float d  = 16.0f;
	float cx = bounds->x + bounds->w * 0.5f;
	float cy = bounds->y + bounds->h * 0.5f;
	if (!flux_rec_fill_circle(b, cx, cy, d, accent)) return false;
	if (!icon_name || !icon_name [0]) return true;
	wchar_t const *icon = flux_icon_lookup(icon_name);
	if (!icon) return true;
	FluxRect ir = {cx - d * 0.5f, cy - d * 0.5f, d, d};
	return flux_rec_glyph_wc(b, &ir, icon, 10.0f, text);
}

bool flux_rec_badge(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	FluxColor              accent = t->accent_default;
	FluxColor              text   = t->text_on_accent_primary;

	switch (snap->u.info_badge.mode) {
	case FLUX_BADGE_DOT :
		return flux_rec_fill_circle(b, bounds->x + bounds->w * 0.5f, bounds->y + bounds->h * 0.5f, 8.0f, accent);
	case FLUX_BADGE_NUMBER : return badge_number(b, bounds, snap->u.info_badge.value, accent, text);
	case FLUX_BADGE_ICON   : return badge_icon(b, bounds, snap->u.info_badge.icon_name, accent, text);
	default                : return true;
	}
}
