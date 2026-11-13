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

static bool themeanim_curve(HTHEME timing, DWORD id, FluxThemeAnim *out) {
	DWORD size = 0;
	if (GetThemeTimingFunction(timing, ( int ) id, NULL, 0, &size) != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) return false;
	TA_TIMINGFUNCTION *fn = ( TA_TIMINGFUNCTION * ) malloc(size);
	bool               ok = fn
	                     && SUCCEEDED(GetThemeTimingFunction(timing, ( int ) id, fn, size, &size))
	                     && fn->eTimingFunctionType == TTFT_CUBIC_BEZIER;
	if (ok) {
		TA_CUBIC_BEZIER const *bz = ( TA_CUBIC_BEZIER const * ) fn;
		out->x1                   = bz->rX0;
		out->y1                   = bz->rY0;
		out->x2                   = bz->rX1;
		out->y2                   = bz->rY1;
	}
	free(fn);
	return ok;
}

static void themeanim_values(TA_TRANSFORM const *tf, FluxThemeAnim *out) {
	if (tf->eTransformType == TATT_OPACITY) {
		TA_TRANSFORM_OPACITY const *op = ( TA_TRANSFORM_OPACITY const * ) tf;
		out->from                      = tf->eFlags & TATF_HASINITIALVALUES ? op->rInitialOpacity : out->from;
		out->to                        = op->rOpacity;
	}
	else if (tf->eTransformType == TATT_SCALE_2D) {
		TA_TRANSFORM_2D const *s = ( TA_TRANSFORM_2D const * ) tf;
		out->from                = tf->eFlags & TATF_HASINITIALVALUES ? s->rInitialX : out->from;
		out->to                  = s->rX;
	}
}

static bool
themeanim_transform(HTHEME anim, HTHEME timing, int sb, int target, DWORD index, int type, FluxThemeAnim *out) {
	DWORD size = 0;
	if (GetThemeAnimationTransform(anim, sb, target, index, NULL, 0, &size) != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		return false;
	TA_TRANSFORM *tf = ( TA_TRANSFORM * ) malloc(size);
	bool          ok = tf
	                && SUCCEEDED(GetThemeAnimationTransform(anim, sb, target, index, tf, size, &size))
	                && ( int ) tf->eTransformType == type;
	if (ok) {
		out->delay    = tf->dwStartTime / 1000.0;
		out->duration = tf->dwDurationTime / 1000.0;
		themeanim_values(tf, out);
		ok = themeanim_curve(timing, tf->dwTimingFunctionId, out);
	}
	free(tf);
	return ok;
}

static bool themeanim_query(int sb, int target, int type, FluxThemeAnim *out) {
	HTHEME anim   = OpenThemeData(NULL, L"Animations");
	HTHEME timing = OpenThemeData(NULL, L"timingfunction");
	DWORD  count = 0, got = 0;
	bool   ok    = anim
	            && timing
	            && SUCCEEDED(GetThemeAnimationProperty(anim, sb, target, TAP_TRANSFORMCOUNT, &count, sizeof(count), &got));
	bool   found = false;
	for (DWORD i = 0; ok && !found && i < count; ++i)
		found = themeanim_transform(anim, timing, sb, target, i, type, out);
	if (anim) CloseThemeData(anim);
	if (timing) CloseThemeData(timing);
	return found;
}

typedef struct ThemeAnimSlot {
	int           sb, target, type;
	bool          ready;
	FluxThemeAnim value;
} ThemeAnimSlot;

FluxThemeAnim flux_themeanim_get(int storyboard, int target, int transform, FluxThemeAnim fallback) {
	static ThemeAnimSlot slots [16];
	for (int i = 0; i < 16; ++i) {
		ThemeAnimSlot *s = &slots [i];
		if (s->ready && s->sb == storyboard && s->target == target && s->type == transform) return s->value;
		if (s->ready) continue;
		FluxThemeAnim v = fallback;
		if (!themeanim_query(storyboard, target, transform, &v)) v = fallback;
		*s = (ThemeAnimSlot) {storyboard, target, transform, true, v};
		return v;
	}
	return fallback;
}

float flux_themeanim_progress(FluxThemeAnim const *a, double elapsed) {
	double t = elapsed - a->delay;
	if (t <= 0.0) return 0.0f;
	if (a->duration <= 0.0 || t >= a->duration) return 1.0f;
	return flux_cubic_bezier(( float ) (t / a->duration), a->x1, a->y1, a->x2, a->y2);
}

double flux_themeanim_end(FluxThemeAnim const *a) { return a->delay + a->duration; }
