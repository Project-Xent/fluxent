#include "controls/behavior/flux_color_picker.h"
#include <math.h>

static uint8_t color_byte(float c) { return ( uint8_t ) lroundf(255.0f * c); }

static void    hsv_sextant(int sextant, float mid, float chroma, float lo, float rgb [3]) {
	float hi = lo + chroma, up = lo + chroma * mid, down = lo + chroma * (1.0f - mid);
	switch (sextant) {
	case 0  : rgb [0] = hi, rgb [1] = up; break;
	case 1  : rgb [0] = down, rgb [1] = hi; break;
	case 2  : rgb [1] = hi, rgb [2] = up; break;
	case 3  : rgb [1] = down, rgb [2] = hi; break;
	case 4  : rgb [0] = up, rgb [2] = hi; break;
	default : rgb [0] = hi, rgb [2] = down; break;
	}
}

FluxColor flux_color_fromhsv(float hue, float saturation, float value, float alpha) {
	while (hue >= 360.0f) hue -= 360.0f;
	while (hue < 0.0f) hue += 360.0f;
	saturation   = fminf(1.0f, fmaxf(0.0f, saturation));
	value        = fminf(1.0f, fmaxf(0.0f, value));
	float chroma = saturation * value, lo = value - chroma;
	float rgb [3] = {lo, lo, lo};
	if (chroma > 0.0f) hsv_sextant(( int ) (hue / 60.0f), hue / 60.0f - floorf(hue / 60.0f), chroma, lo, rgb);
	return flux_color_rgba(color_byte(rgb [0]), color_byte(rgb [1]), color_byte(rgb [2]), color_byte(alpha));
}

void flux_color_tohsv(FluxColor color, float *hue, float *saturation, float *value) {
	float r = flux_color_rf(color), g = flux_color_gf(color), b = flux_color_bf(color);
	float hi = fmaxf(r, fmaxf(g, b)), lo = fminf(r, fminf(g, b)), chroma = hi - lo;
	*value      = hi;
	*hue        = 0.0f;
	*saturation = 0.0f;
	if (chroma == 0.0f) return;
	float h     = r == hi ? 60.0f * (g - b) / chroma
	            : g == hi ? 120.0f + 60.0f * (b - r) / chroma
	                      : 240.0f + 60.0f * (r - g) / chroma;
	*hue        = h < 0.0f ? h + 360.0f : h;
	*saturation = chroma / hi;
}
