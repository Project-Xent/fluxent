#ifndef FLUX_FONT_ICON_DATA_H
#define FLUX_FONT_ICON_DATA_H

#include <fluxent/flux_text.h>
#include <fluxent/flux_types.h>
#include <stdbool.h>

typedef struct FluxFontIconData {
	char const    *glyph;
	char const    *font_family;
	FluxColor      color;
	float          font_size;
	FluxFontWeight font_weight;
	bool           mirrored_when_rtl;
} FluxFontIconData;

#endif
