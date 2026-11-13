#ifndef FLUX_SLIDER_GEOM_H
#define FLUX_SLIDER_GEOM_H

#include <math.h>
#include <stdbool.h>

#define FLUX_SLIDER_THUMB_TRAVEL 9.0f
#define FLUX_SLIDER_THUMB_OUTER  11.0f

typedef struct FluxSliderLine {
	bool  vertical;
	float start;
	float length;
	float cross;
} FluxSliderLine;

static FluxSliderLine inline flux_slider_line(float x, float y, float w, float h, bool vertical) {
	float main = vertical ? h : w;
	float len  = main - FLUX_SLIDER_THUMB_TRAVEL * 2.0f;
	return ( FluxSliderLine ) {
	  .vertical = vertical,
	  .start    = (vertical ? y : x) + FLUX_SLIDER_THUMB_TRAVEL,
	  .length   = len < 0.0f ? 0.0f : len,
	  .cross    = vertical ? x + w * 0.5f : y + h * 0.5f,
	};
}

static float inline flux_slider_along(FluxSliderLine const *line, float fraction) {
	return line->start + line->length * (line->vertical ? 1.0f - fraction : fraction);
}

static float inline flux_slider_fraction_at(FluxSliderLine const *line, float px, float py) {
	float t = ((line->vertical ? py : px) - line->start) / fmaxf(line->length, 1.0f);
	t       = fminf(1.0f, fmaxf(0.0f, t));
	return line->vertical ? 1.0f - t : t;
}

#endif
