#ifndef FLUX_COLOR_PICKER_GEOM_H
#define FLUX_COLOR_PICKER_GEOM_H
#include "controls/behavior/flux_color_picker.h"

#define FLUX_CP_PAD       4.0f
#define FLUX_CP_SPECTRUM  256.0f
#define FLUX_CP_PREVIEW_X 268.0f
#define FLUX_CP_PREVIEW_W 44.0f
#define FLUX_CP_CONTENT_W 312.0f
#define FLUX_CP_SLIDER_H  32.0f
#define FLUX_CP_TRACK_H   12.0f
#define FLUX_CP_FIELD_W   120.0f
#define FLUX_CP_HEX_W     132.0f
#define FLUX_CP_ROW_H     32.0f
#define FLUX_CP_ROW_GAP   12.0f
#define FLUX_CP_LABEL_X   128.0f
#define FLUX_CP_THUMB     18.0f
#define FLUX_CP_MAX_HUE   359.0f
#define FLUX_CP_MORE_W    120.0f

typedef struct FluxColorPickerGeom {
	float third_y, alpha_y, more_y, entries_y, height;
	bool  entries;
} FluxColorPickerGeom;

static FluxColorPickerGeom inline flux_colorpicker_geom(bool alpha, bool more_button, bool opened) {
	FluxColorPickerGeom g = {0};
	g.third_y             = FLUX_CP_PAD + FLUX_CP_SPECTRUM + 16.0f;
	g.alpha_y             = g.third_y + FLUX_CP_SLIDER_H + 6.0f;
	g.more_y              = alpha ? g.alpha_y + FLUX_CP_SLIDER_H + 16.0f : g.alpha_y;
	g.entries_y           = more_button ? g.more_y + FLUX_CP_ROW_H : g.more_y;
	g.entries             = !more_button || opened;
	float rows            = FLUX_CP_ROW_H + 3.0f * (FLUX_CP_ROW_GAP + FLUX_CP_ROW_H);
	if (alpha) rows += FLUX_CP_ROW_GAP + FLUX_CP_ROW_H;
	g.height = g.entries_y + (g.entries ? rows : 0.0f) + 12.0f + FLUX_CP_PAD;
	return g;
}

static FluxColorPickerGeom inline flux_colorpicker_geom_of(FluxColorPickerSnapshot const *c) {
	return flux_colorpicker_geom(c->alpha, c->more_button, c->opened);
}

static float inline flux_colorpicker_row_y(FluxColorPickerGeom const *g, int row) {
	return g->entries_y + FLUX_CP_ROW_H + FLUX_CP_ROW_GAP + ( float ) row * (FLUX_CP_ROW_GAP + FLUX_CP_ROW_H);
}
#endif
