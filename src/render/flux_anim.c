#include "render/flux_anim.h"
#include <math.h>

static float anim_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float        flux_cubic_bezier(float x, float x1, float y1, float x2, float y2) {
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
