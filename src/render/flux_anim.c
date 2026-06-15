/**
 * @file flux_anim.c
 * @brief Out-of-line animation helpers shared across controls.
 *
 * The cubic-bezier easing solver lives here (not as a header inline) so the
 * Newton-iteration loop is emitted exactly once and shared by every caller
 * (nav view indicator, popup show/flyout curves, ...) instead of each
 * translation unit carrying its own ~1.3 KB copy.
 */

#include "render/flux_anim.h"
#include <math.h>

static float anim_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Unit cubic-bezier ease (P0=0, P3=1): for input x in [0,1], Newton-solve the
 * parameter t such that bezier_x(t)=x, then return bezier_y(t). Six iterations
 * converge well within sub-pixel tolerance for UI timing curves. */
float flux_cubic_bezier(float x, float x1, float y1, float x2, float y2) {
	float t = anim_clamp01(x);
	for (int i = 0; i < 6; i++) {
		float u  = 1.0f - t;
		float fx = 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t;
		float dx = 3.0f * u * u * x1 + 6.0f * u * t * (x2 - x1) + 3.0f * t * t * (1.0f - x2);
		if (fabsf(dx) < 1e-5f) break;
		t -= (fx - x) / dx;
		t  = anim_clamp01(t);
	}
	float u = 1.0f - t;
	return 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t;
}
