#include "theme/flux_theme_anim.h"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <stdlib.h>

_Static_assert(
  ( int ) FLUX_THEMEANIM_OPACITY == ( int ) TATT_OPACITY && ( int ) FLUX_THEMEANIM_CLIP == ( int ) TATT_CLIP,
  "TA_TRANSFORM_TYPE"
);

#include "render/flux_anim.h"

static bool themeanim_curve_pts(HTHEME timing, DWORD id, float *x1, float *y1, float *x2, float *y2) {
	DWORD size = 0;
	if (GetThemeTimingFunction(timing, ( int ) id, NULL, 0, &size) != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) return false;
	TA_TIMINGFUNCTION *fn = ( TA_TIMINGFUNCTION * ) malloc(size);
	bool               ok = fn
	                     && SUCCEEDED(GetThemeTimingFunction(timing, ( int ) id, fn, size, &size))
	                     && fn->eTimingFunctionType == TTFT_CUBIC_BEZIER;
	if (ok) {
		TA_CUBIC_BEZIER const *bz = ( TA_CUBIC_BEZIER const * ) fn;
		*x1                       = bz->rX0;
		*y1                       = bz->rY0;
		*x2                       = bz->rX1;
		*y2                       = bz->rY1;
	}
	free(fn);
	return ok;
}

static void themeanim_track_values(TA_TRANSFORM const *tf, FluxThemeTrack *out) {
	if (tf->eTransformType == TATT_OPACITY) {
		TA_TRANSFORM_OPACITY const *op = ( TA_TRANSFORM_OPACITY const * ) tf;
		out->has_initial               = (tf->eFlags & TATF_HASINITIALVALUES) != 0;
		if (out->has_initial) out->init_x = op->rInitialOpacity;
		out->to_x = op->rOpacity;
		return;
	}
	if (tf->eTransformType != TATT_SCALE_2D
	    && tf->eTransformType != TATT_TRANSLATE_2D
	    && tf->eTransformType != TATT_CLIP)
		return;
	TA_TRANSFORM_2D const *t2 = ( TA_TRANSFORM_2D const * ) tf;
	out->has_initial          = (tf->eFlags & TATF_HASINITIALVALUES) != 0;
	out->has_origin           = (tf->eFlags & TATF_HASORIGINVALUES) != 0;
	if (out->has_initial) {
		out->init_x = t2->rInitialX;
		out->init_y = t2->rInitialY;
	}
	if (out->has_origin) {
		out->origin_x = t2->rOriginX;
		out->origin_y = t2->rOriginY;
	}
	if ((tf->eFlags & TATF_TARGETVALUES_USER) == 0) {
		out->to_x = t2->rX;
		out->to_y = t2->rY;
	}
}

static bool
themeanim_transform_track(HTHEME anim, HTHEME timing, int sb, int target, DWORD index, int type, FluxThemeTrack *out) {
	DWORD size = 0;
	if (GetThemeAnimationTransform(anim, sb, target, index, NULL, 0, &size) != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		return false;
	TA_TRANSFORM *tf = ( TA_TRANSFORM * ) malloc(size);
	bool          ok = tf
	                && SUCCEEDED(GetThemeAnimationTransform(anim, sb, target, index, tf, size, &size))
	                && ( int ) tf->eTransformType == type;
	if (ok) {
		out->found    = true;
		out->delay    = tf->dwStartTime / 1000.0;
		out->duration = tf->dwDurationTime / 1000.0;
		themeanim_track_values(tf, out);
		ok = themeanim_curve_pts(timing, tf->dwTimingFunctionId, &out->x1, &out->y1, &out->x2, &out->y2);
	}
	free(tf);
	return ok;
}

static bool themeanim_query_track(int sb, int target, int type, FluxThemeTrack *out) {
	HTHEME anim   = OpenThemeData(NULL, L"Animations");
	HTHEME timing = OpenThemeData(NULL, L"timingfunction");
	DWORD  count = 0, got = 0;
	bool   ok    = anim
	            && timing
	            && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_TRANSFORMCOUNT, &count, sizeof(count), &got));
	bool   found = false;
	for (DWORD i = 0; ok && !found && i < count; ++i)
		found = themeanim_transform_track(anim, timing, sb, target, i, type, out);
	if (anim) CloseThemeData(anim);
	if (timing) CloseThemeData(timing);
	return found;
}

typedef struct ThemeTrackSlot {
	int            sb, target, type;
	bool           ready;
	FluxThemeTrack value;
} ThemeTrackSlot;

typedef struct ThemeStaggerSlot {
	int              sb, target;
	bool             ready;
	FluxThemeStagger value;
} ThemeStaggerSlot;

FluxThemeTrack flux_themeanim_track(int storyboard, int target, int transform, FluxThemeTrack fallback) {
	static ThemeTrackSlot slots [32];
	for (int i = 0; i < 32; ++i) {
		ThemeTrackSlot *s = &slots [i];
		if (s->ready && s->sb == storyboard && s->target == target && s->type == transform) return s->value;
		if (s->ready) continue;
		FluxThemeTrack v = fallback;
		if (!themeanim_query_track(storyboard, target, transform, &v)) v = fallback;
		*s = (ThemeTrackSlot) {storyboard, target, transform, true, v};
		return v;
	}
	return fallback;
}

static bool themeanim_query_stagger(int sb, int target, FluxThemeStagger *out) {
	HTHEME anim  = OpenThemeData(NULL, L"Animations");
	DWORD  flags = 0, delay = 0, cap = 0, got = 0;
	FLOAT  factor = 1.0f;
	bool   ok
	  = anim
	 && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_FLAGS, &flags, sizeof(flags), &got))
	 && (flags & TAPF_HASSTAGGER)
	 && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_STAGGERDELAY, &delay, sizeof(delay), &got))
	 && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_STAGGERDELAYCAP, &cap, sizeof(cap), &got))
	 && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_STAGGERDELAYFACTOR, &factor, sizeof(factor), &got));
	if (anim) CloseThemeData(anim);
	if (!ok) return false;
	out->valid      = true;
	out->delay_ms   = ( double ) delay;
	out->maximum_ms = ( double ) cap;
	out->reduce     = ( double ) factor;
	return true;
}

FluxThemeStagger flux_themeanim_stagger(int storyboard, int target, FluxThemeStagger fallback) {
	static ThemeStaggerSlot slots [16];
	for (int i = 0; i < 16; ++i) {
		ThemeStaggerSlot *s = &slots [i];
		if (s->ready && s->sb == storyboard && s->target == target) return s->value;
		if (s->ready) continue;
		FluxThemeStagger v = fallback;
		if (!themeanim_query_stagger(storyboard, target, &v)) v = fallback;
		*s = (ThemeStaggerSlot) {storyboard, target, true, v};
		return v;
	}
	return fallback;
}

double flux_themeanim_stagger_delay(FluxThemeStagger const *s, uint32_t index) {
	if (!s || !s->valid || s->delay_ms <= 0.0) return 0.0;
	double step  = s->delay_ms;
	double total = step;
	for (uint32_t i = 0; i < index; i++) {
		step  *= s->reduce;
		total += step;
	}
	if (s->maximum_ms > 0.0 && total > s->maximum_ms) total = s->maximum_ms;
	return total / 1000.0;
}

FluxThemeAnim flux_themeanim_get(int storyboard, int target, int transform, FluxThemeAnim fallback) {
	FluxThemeTrack tr_fb = {
	  .found       = false,
	  .delay       = fallback.delay,
	  .duration    = fallback.duration,
	  .x1          = fallback.x1,
	  .y1          = fallback.y1,
	  .x2          = fallback.x2,
	  .y2          = fallback.y2,
	  .has_initial = true,
	  .init_x      = fallback.from,
	  .to_x        = fallback.to,
	};
	FluxThemeTrack tr = flux_themeanim_track(storyboard, target, transform, tr_fb);
	return (FluxThemeAnim) {
	  .delay    = tr.delay,
	  .duration = tr.duration,
	  .x1       = tr.x1,
	  .y1       = tr.y1,
	  .x2       = tr.x2,
	  .y2       = tr.y2,
	  .from     = tr.has_initial ? tr.init_x : fallback.from,
	  .to       = tr.to_x,
	};
}

float flux_themeanim_progress(FluxThemeAnim const *a, double elapsed) {
	double t = elapsed - a->delay;
	if (t <= 0.0) return 0.0f;
	if (a->duration <= 0.0 || t >= a->duration) return 1.0f;
	return flux_cubic_bezier(( float ) (t / a->duration), a->x1, a->y1, a->x2, a->y2);
}

double flux_themeanim_end(FluxThemeAnim const *a) { return a->delay + a->duration; }
