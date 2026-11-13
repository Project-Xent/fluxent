#ifndef FLUX_FLUENT_H
#define FLUX_FLUENT_H

#include "flux_render_internal.h"
#include <math.h>

#define FLUX_CORNER_RADIUS             4.0f
#define FLUX_STROKE_WIDTH              1.0f
#define FLUX_PIXEL_SNAP                0.5f
#define FLUX_FONT_SIZE_DEFAULT         14.0f
#define FLUX_VISIBILITY_THRESHOLD      0.01f

#define FLUX_FOCUS_OUTER_PAD           2.0f
#define FLUX_FOCUS_OUTER_THICK         2.0f
#define FLUX_FOCUS_INNER_PAD           1.0f
#define FLUX_FOCUS_INNER_THICK         1.0f

#define FLUX_BTN_PAD_TOP               5.0f
#define FLUX_BTN_PAD_RIGHT             11.0f
#define FLUX_BTN_PAD_BOTTOM            6.0f
#define FLUX_BTN_PAD_LEFT              11.0f
#define FLUX_BTN_CONTENT_GAP           8.0f
#define FLUX_BTN_ICON_SIZE_MUL         1.14f

#define FLUX_SLIDER_INNER_THUMB        12.0f
#define FLUX_SLIDER_SCALE_NORMAL       0.86f
#define FLUX_SLIDER_SCALE_HOVER        1.167f
#define FLUX_SLIDER_SCALE_PRESSED      0.71f

#define FLUX_TOGGLE_WIDTH              40.0f
#define FLUX_TOGGLE_HEIGHT             20.0f
#define FLUX_TOGGLE_KNOB_NORMAL        12.0f
#define FLUX_TOGGLE_KNOB_HOVER         14.0f
#define FLUX_TOGGLE_KNOB_PRESS_W       17.0f
#define FLUX_TOGGLE_KNOB_PRESS_H       14.0f
#define FLUX_TOGGLE_TRAVEL_EXT         3.0f

#define FLUX_CHECKBOX_SIZE             20.0f
#define FLUX_CHECKBOX_CORNER           4.0f
#define FLUX_CHECKBOX_GAP              12.0f
#define FLUX_CHECKBOX_GLYPH_SIZE       12.0f

#define FLUX_RADIO_SIZE                20.0f
#define FLUX_RADIO_GAP                 12.0f
#define FLUX_RADIO_GLYPH               6.0f
#define FLUX_RADIO_GLYPH_HOVER         7.0f
#define FLUX_RADIO_GLYPH_PRESSED       5.0f
#define FLUX_RADIO_PRESS_GLYPH         10.0f

#define FLUX_SCROLLBAR_SIZE            12.0f
#define FLUX_SCROLLBAR_PAD             2.0f
#define FLUX_SCROLLBAR_MIN_THUMB       20.0f
#define FLUX_SCROLL_VIS_EPS            0.5f

#define FLUX_TEXTBOX_PAD_L             10.0f
#define FLUX_TEXTBOX_PAD_T             5.0f
#define FLUX_TEXTBOX_PAD_R             6.0f
#define FLUX_TEXTBOX_PAD_B             6.0f
#define FLUX_COMBO_GLYPH_COL           38.0f
#define FLUX_COMBO_EDITOR_RESERVE      32.0f

#define FLUX_ELEVATION_HEIGHT          3.0f
#define FLUX_ELEVATION_STOP1           0.33f
#define FLUX_ELEVATION_STOP2           1.0f

#define FLUX_INTERACTION_PRESSED_SCALE 0.9f
#define FLUX_INTERACTION_PRESSED_ALPHA 0.8f
#define FLUX_INTERACTION_HOVER_ALPHA   0.9f

#define FLUX_ANIM_FAST                 0.083
#define FLUX_ANIM_NORMAL               0.167
#define FLUX_ANIM_SLOW                 0.250
#define FLUX_ANIM_VALUE_EPS            0.001f

typedef struct FluxStateColorMap {
	FluxColor disabled;
	FluxColor focused;
	FluxColor hovered;
	FluxColor normal;
} FluxStateColorMap;

static FluxColor inline flux_resolve_state_color(FluxStateColorMap const *m, bool enabled, bool focused, bool hovered) {
	if (!enabled) return m->disabled;
	if (focused) return m->focused;
	if (hovered) return m->hovered;
	return m->normal;
}

static FluxColor inline flux_input_fill_color(FluxThemeColors const *t, bool enabled, bool focused, bool hovered) {
	FluxStateColorMap m = {
	  .disabled = t->ctrl_fill_disabled,
	  .focused  = t->ctrl_fill_input_active,
	  .hovered  = t->ctrl_fill_secondary,
	  .normal   = t->ctrl_fill_default,
	};
	return flux_resolve_state_color(&m, enabled, focused, hovered);
}

static float inline flux_clamp01(float v) {
	if (v < 0.0f) return 0.0f;
	if (v > 1.0f) return 1.0f;
	return v;
}

static float inline flux_clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static float inline flux_maxf(float a, float b) { return a > b ? a : b; }

static float inline flux_minf(float a, float b) { return a < b ? a : b; }

static FluxRect inline flux_snap_bounds(FluxRect const *bounds, float sx, float sy) {
	float    x0 = floorf(bounds->x * sx + FLUX_PIXEL_SNAP) / sx;
	float    y0 = floorf(bounds->y * sy + FLUX_PIXEL_SNAP) / sy;
	float    x1 = floorf((bounds->x + bounds->w) * sx + FLUX_PIXEL_SNAP) / sx;
	float    y1 = floorf((bounds->y + bounds->h) * sy + FLUX_PIXEL_SNAP) / sy;
	FluxRect r  = {x0, y0, x1 - x0, y1 - y0};
	return r;
}

typedef struct FluxCheckboxLayout {
	FluxRect box_rect;
	FluxRect text_rect;
	float    glyph_size;
} FluxCheckboxLayout;

static FluxColor inline flux_elevation_secondary(FluxRenderCtx const *rc, bool is_accent) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_defaults();
	return is_accent ? t->ctrl_stroke_on_accent_secondary : t->ctrl_stroke_secondary;
}

static FluxColor inline flux_elevation_primary(FluxRenderCtx const *rc, bool is_accent) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_defaults();
	return is_accent ? t->ctrl_stroke_on_accent_default : t->ctrl_stroke_default;
}

static void inline flux_elevation_band(FluxRect const *bounds, bool flip, float *band_start, float *band_end) {
	float band_h = FLUX_ELEVATION_HEIGHT;
	if (flip) *band_start = bounds->y + bounds->h - band_h;
	else *band_start = bounds->y;
	*band_end = *band_start + band_h;
}

static D2D1_BRUSH_PROPERTIES inline flux_default_brush_props(void) {
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	return bp;
}

#endif
