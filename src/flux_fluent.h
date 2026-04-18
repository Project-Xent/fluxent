/**
 * @file flux_fluent.h
 * @brief WinUI 3 Fluent Design constants and rendering utilities.
 *
 * This header provides:
 * - Design token constants (sizes, padding, animation durations)
 * - Fallback color functions (when FluxThemeColors is unavailable)
 * - Common rendering helpers (rounded rects, focus rings, elevation)
 *
 * @note All dimensions are in DIPs (device-independent pixels).
 */
#ifndef FLUX_FLUENT_H
#define FLUX_FLUENT_H

#include "flux_render_internal.h"
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
   Design Token Constants
   ═══════════════════════════════════════════════════════════════════════════ */

#define FLUX_CORNER_RADIUS             4.0f  /**< Default control corner radius */
#define FLUX_STROKE_WIDTH              1.0f  /**< Default border stroke width */
#define FLUX_PIXEL_SNAP                0.5f  /**< Sub-pixel offset for crisp lines */
#define FLUX_FONT_SIZE_DEFAULT         14.0f /**< BodyTextBlockStyle font size */
#define FLUX_VISIBILITY_THRESHOLD      0.01f /**< Minimum alpha for visibility */

/* Focus visual dimensions */
#define FLUX_FOCUS_OUTER_PAD           2.0f
#define FLUX_FOCUS_OUTER_THICK         2.0f
#define FLUX_FOCUS_INNER_PAD           1.0f
#define FLUX_FOCUS_INNER_THICK         1.0f

/* Button padding and spacing */
#define FLUX_BTN_PAD_TOP               5.0f
#define FLUX_BTN_PAD_RIGHT             11.0f
#define FLUX_BTN_PAD_BOTTOM            6.0f
#define FLUX_BTN_PAD_LEFT              11.0f
#define FLUX_BTN_CONTENT_GAP           8.0f
#define FLUX_BTN_ICON_SIZE_MUL         1.14f

/* Slider dimensions */
#define FLUX_SLIDER_TRACK_H            4.0f
#define FLUX_SLIDER_THUMB_SIZE         18.0f
#define FLUX_SLIDER_INNER_THUMB        12.0f
#define FLUX_SLIDER_SCALE_NORMAL       0.86f
#define FLUX_SLIDER_SCALE_HOVER        1.0f
#define FLUX_SLIDER_SCALE_PRESSED      0.71f

/* ToggleSwitch dimensions */
#define FLUX_TOGGLE_WIDTH              40.0f
#define FLUX_TOGGLE_HEIGHT             20.0f
#define FLUX_TOGGLE_KNOB_NORMAL        12.0f
#define FLUX_TOGGLE_KNOB_HOVER         14.0f
#define FLUX_TOGGLE_KNOB_PRESS_W       17.0f
#define FLUX_TOGGLE_KNOB_PRESS_H       14.0f
#define FLUX_TOGGLE_TRAVEL_EXT         3.0f

/* Checkbox dimensions */
#define FLUX_CHECKBOX_SIZE             20.0f
#define FLUX_CHECKBOX_CORNER           4.0f
#define FLUX_CHECKBOX_GAP              12.0f
#define FLUX_CHECKBOX_GLYPH_SIZE       12.0f

/* RadioButton dimensions */
#define FLUX_RADIO_SIZE                20.0f
#define FLUX_RADIO_GAP                 12.0f
#define FLUX_RADIO_GLYPH               6.0f
#define FLUX_RADIO_GLYPH_HOVER         7.0f
#define FLUX_RADIO_GLYPH_PRESSED       5.0f
#define FLUX_RADIO_PRESS_GLYPH         10.0f

/* ScrollBar dimensions */
#define FLUX_SCROLLBAR_SIZE            12.0f
#define FLUX_SCROLLBAR_PAD             2.0f
#define FLUX_SCROLLBAR_MIN_THUMB       20.0f
#define FLUX_SCROLL_VIS_EPS            0.5f

/* TextBox padding */
#define FLUX_TEXTBOX_PAD_L             10.0f
#define FLUX_TEXTBOX_PAD_T             5.0f
#define FLUX_TEXTBOX_PAD_R             6.0f
#define FLUX_TEXTBOX_PAD_B             6.0f

/* Elevation shadow parameters */
#define FLUX_ELEVATION_HEIGHT          3.0f
#define FLUX_ELEVATION_STOP1           0.33f
#define FLUX_ELEVATION_STOP2           1.0f

/* Interaction feedback */
#define FLUX_INTERACTION_PRESSED_SCALE 0.9f
#define FLUX_INTERACTION_PRESSED_ALPHA 0.8f
#define FLUX_INTERACTION_HOVER_ALPHA   0.9f

/* Animation durations (seconds) */
#define FLUX_ANIM_FAST                 0.083  /**< ~83ms — quick transitions */
#define FLUX_ANIM_NORMAL               0.167  /**< ~167ms — hover/focus */
#define FLUX_ANIM_SLOW                 0.250  /**< ~250ms — color fades */
#define FLUX_ANIM_VALUE_EPS            0.001f /**< Threshold for "close enough" */

/* ═══════════════════════════════════════════════════════════════════════════
   Fallback Color Functions (Light Theme)

   These inline functions provide WinUI 3 light theme colors when
   FluxThemeColors is unavailable. Names follow the WinUI token naming.
   ═══════════════════════════════════════════════════════════════════════════ */

/* Control fill colors */
static FluxColor inline ft_ctrl_fill_default(void) { return flux_color_rgba(255, 255, 255, 0xb3); }

static FluxColor inline ft_ctrl_fill_secondary(void) { return flux_color_rgba(249, 249, 249, 0x80); }

static FluxColor inline ft_ctrl_fill_tertiary(void) { return flux_color_rgba(249, 249, 249, 0x4d); }

static FluxColor inline ft_ctrl_fill_disabled(void) { return flux_color_rgba(249, 249, 249, 0x4d); }

static FluxColor inline ft_ctrl_fill_input_active(void) { return flux_color_rgba(255, 255, 255, 0xff); }

/* Control alt fill colors */
static FluxColor inline ft_ctrl_alt_fill_secondary(void) { return flux_color_rgba(0, 0, 0, 0x06); }

static FluxColor inline ft_ctrl_alt_fill_tertiary(void) { return flux_color_rgba(0, 0, 0, 0x0f); }

static FluxColor inline ft_ctrl_alt_fill_quarternary(void) { return flux_color_rgba(0, 0, 0, 0x12); }

static FluxColor inline ft_ctrl_alt_fill_disabled(void) { return flux_color_rgba(0, 0, 0, 0x00); }

/* Subtle fill colors */
static FluxColor inline ft_subtle_fill_secondary(void) { return flux_color_rgba(0, 0, 0, 0x09); }

static FluxColor inline ft_subtle_fill_tertiary(void) { return flux_color_rgba(0, 0, 0, 0x06); }

/* Control stroke colors */
static FluxColor inline ft_ctrl_stroke_default(void) { return flux_color_rgba(0, 0, 0, 0x0f); }

static FluxColor inline ft_ctrl_stroke_secondary(void) { return flux_color_rgba(0, 0, 0, 0x29); }

static FluxColor inline ft_ctrl_stroke_on_accent_default(void) { return flux_color_rgba(0, 0, 0, 0x24); }

static FluxColor inline ft_ctrl_stroke_on_accent_secondary(void) { return flux_color_rgba(255, 255, 255, 0x14); }

static FluxColor inline ft_ctrl_strong_stroke_default(void) { return flux_color_rgba(0, 0, 0, 0x9c); }

static FluxColor inline ft_ctrl_strong_stroke_disabled(void) { return flux_color_rgba(0, 0, 0, 0x37); }

/* Solid/strong fill colors */
static FluxColor inline ft_ctrl_solid_fill_default(void) { return flux_color_rgba(255, 255, 255, 0xff); }

static FluxColor inline ft_ctrl_strong_fill_default(void) { return flux_color_rgba(0, 0, 0, 0x9c); }

static FluxColor inline ft_ctrl_strong_fill_disabled(void) { return flux_color_rgba(0, 0, 0, 0x51); }

/* Accent colors */
static FluxColor inline ft_accent_default(void) { return flux_color_rgb(0, 120, 212); }

static FluxColor inline ft_accent_secondary(void) { return flux_color_rgba(0, 120, 212, 0xe6); }

static FluxColor inline ft_accent_tertiary(void) { return flux_color_rgba(0, 120, 212, 0xcc); }

static FluxColor inline ft_accent_disabled(void) { return flux_color_rgba(0, 0, 0, 0x37); }

static FluxColor inline ft_text_primary(void) { return flux_color_rgba(0, 0, 0, 0xe4); }

static FluxColor inline ft_text_secondary(void) { return flux_color_rgba(0, 0, 0, 0x9e); }

static FluxColor inline ft_text_disabled(void) { return flux_color_rgba(0, 0, 0, 0x5c); }

static FluxColor inline ft_text_on_accent_primary(void) { return flux_color_rgb(255, 255, 255); }

static FluxColor inline ft_text_on_accent_secondary(void) { return flux_color_rgba(255, 255, 255, 0xb3); }

static FluxColor inline ft_text_on_accent_disabled(void) { return flux_color_rgb(255, 255, 255); }

static FluxColor inline ft_card_bg_default(void) { return flux_color_rgba(255, 255, 255, 0xb3); }

static FluxColor inline ft_card_stroke_default(void) { return flux_color_rgba(0, 0, 0, 0x0f); }

static FluxColor inline ft_divider_stroke_default(void) { return flux_color_rgba(0, 0, 0, 0x14); }

static FluxColor inline ft_focus_stroke_outer(void) { return flux_color_rgba(0, 0, 0, 0xe4); }

static FluxColor inline ft_focus_stroke_inner(void) { return flux_color_rgb(255, 255, 255); }

static FluxColor inline ft_transparent(void) { return flux_color_rgba(0, 0, 0, 0); }

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

static float inline flux_srgb_to_linear(float c) {
	if (c <= 0.04045f) return c / 12.92f;
	return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float inline flux_linear_to_srgb(float c) {
	if (c <= 0.0031308f) return 12.92f * c;
	return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static FluxColor inline flux_lerp_color_srgb(FluxColor a, FluxColor b, float t) {
	t        = flux_clamp01(t);
	float ar = flux_color_rf(a), ag = flux_color_gf(a), ab = flux_color_bf(a), aa = flux_color_af(a);
	float br = flux_color_rf(b), bg = flux_color_gf(b), bb = flux_color_bf(b), ba = flux_color_af(b);

	float lr = flux_srgb_to_linear(ar) + (flux_srgb_to_linear(br) - flux_srgb_to_linear(ar)) * t;
	float lg = flux_srgb_to_linear(ag) + (flux_srgb_to_linear(bg) - flux_srgb_to_linear(ag)) * t;
	float lb = flux_srgb_to_linear(ab) + (flux_srgb_to_linear(bb) - flux_srgb_to_linear(ab)) * t;
	float la = aa + (ba - aa) * t;

	float sr = flux_clamp01(flux_linear_to_srgb(lr));
	float sg = flux_clamp01(flux_linear_to_srgb(lg));
	float sb = flux_clamp01(flux_linear_to_srgb(lb));
	float sa = flux_clamp01(la);

	return flux_color_rgba(
	  ( uint8_t ) (sr * 255.0f + 0.5f), ( uint8_t ) (sg * 255.0f + 0.5f), ( uint8_t ) (sb * 255.0f + 0.5f),
	  ( uint8_t ) (sa * 255.0f + 0.5f)
	);
}

static FluxRect inline flux_snap_bounds(FluxRect const *bounds, float sx, float sy) {
	float    x0 = floorf(bounds->x * sx + FLUX_PIXEL_SNAP) / sx;
	float    y0 = floorf(bounds->y * sy + FLUX_PIXEL_SNAP) / sy;
	float    x1 = floorf((bounds->x + bounds->w) * sx + FLUX_PIXEL_SNAP) / sx;
	float    y1 = floorf((bounds->y + bounds->h) * sy + FLUX_PIXEL_SNAP) / sy;
	FluxRect r  = {x0, y0, x1 - x0, y1 - y0};
	return r;
}

static void inline flux_draw_focus_rect(FluxRenderContext const *rc, FluxRect const *bounds, float radius) {
	FluxRect outer = {
	  bounds->x - FLUX_FOCUS_OUTER_PAD, bounds->y - FLUX_FOCUS_OUTER_PAD, bounds->w + FLUX_FOCUS_OUTER_PAD * 2.0f,
	  bounds->h + FLUX_FOCUS_OUTER_PAD * 2.0f};
	float outer_r = radius + FLUX_FOCUS_OUTER_PAD - 1.0f;
	if (outer_r < 0.0f) outer_r = 0.0f;
	flux_draw_rounded_rect(rc, &outer, outer_r, ft_focus_stroke_outer(), FLUX_FOCUS_OUTER_THICK);

	FluxRect inner = {
	  bounds->x - FLUX_FOCUS_INNER_PAD, bounds->y - FLUX_FOCUS_INNER_PAD, bounds->w + FLUX_FOCUS_INNER_PAD * 2.0f,
	  bounds->h + FLUX_FOCUS_INNER_PAD * 2.0f};
	float inner_r = radius + FLUX_FOCUS_INNER_PAD;
	flux_draw_rounded_rect(rc, &inner, inner_r, ft_focus_stroke_inner(), FLUX_FOCUS_INNER_THICK);
}

typedef struct FluxSliderLayout {
	FluxRect track_rect;
	FluxRect value_rect;
	FluxRect thumb_rect;
	float    thumb_center_x;
	float    track_width;
	float    track_start_x;
} FluxSliderLayout;

static FluxSliderLayout inline flux_calc_slider_layout(
  FluxRect const *bounds, float min_val, float max_val, float value
) {
	FluxSliderLayout l;
	float            val         = flux_clampf(value, min_val, max_val);
	float            range       = max_val - min_val;
	float            pct         = (range > 0.0f) ? (val - min_val) / range : 0.0f;

	float            cy          = bounds->y + bounds->h * 0.5f;
	float            outer_r     = FLUX_SLIDER_THUMB_SIZE * 0.5f;
	float            track_left  = bounds->x + outer_r;
	float            track_right = bounds->x + bounds->w - outer_r;
	float            track_w     = flux_maxf(0.0f, track_right - track_left);

	l.track_width                = track_w;
	l.track_start_x              = track_left;
	l.thumb_center_x             = track_left + pct * track_w;

	l.track_rect.x               = bounds->x;
	l.track_rect.y               = cy - FLUX_SLIDER_TRACK_H * 0.5f;
	l.track_rect.w               = bounds->w;
	l.track_rect.h               = FLUX_SLIDER_TRACK_H;

	l.value_rect.x               = bounds->x;
	l.value_rect.y               = cy - FLUX_SLIDER_TRACK_H * 0.5f;
	l.value_rect.w               = l.thumb_center_x - bounds->x;
	l.value_rect.h               = FLUX_SLIDER_TRACK_H;

	l.thumb_rect.x               = l.thumb_center_x - outer_r;
	l.thumb_rect.y               = cy - outer_r;
	l.thumb_rect.w               = FLUX_SLIDER_THUMB_SIZE;
	l.thumb_rect.h               = FLUX_SLIDER_THUMB_SIZE;

	return l;
}

typedef struct FluxCheckboxLayout {
	FluxRect box_rect;
	FluxRect text_rect;
	float    glyph_size;
} FluxCheckboxLayout;

static FluxCheckboxLayout inline flux_calc_checkbox_layout(
  FluxRect const *bounds, float size, float gap, float glyph_font_size
) {
	FluxCheckboxLayout l;
	float              cy = bounds->y + bounds->h * 0.5f;
	l.box_rect.x          = bounds->x;
	l.box_rect.y          = cy - size * 0.5f;
	l.box_rect.w          = size;
	l.box_rect.h          = size;
	l.glyph_size          = glyph_font_size;
	l.text_rect.x         = bounds->x + size + gap;
	l.text_rect.y         = bounds->y;
	l.text_rect.w         = flux_maxf(0.0f, bounds->w - (size + gap));
	l.text_rect.h         = bounds->h;
	return l;
}

typedef struct FluxScrollBarLayout {
	FluxRect bar_rect;
	FluxRect track_rect;
	FluxRect thumb_rect;
	bool     visible;
} FluxScrollBarLayout;

typedef struct FluxScrollViewLayout {
	FluxScrollBarLayout h_bar;
	FluxScrollBarLayout v_bar;
	float               content_width;
	float               content_height;
} FluxScrollViewLayout;

static bool inline flux_scroll_should_show(int vis, float content, float viewport) {
	if (vis == FLUX_SCROLL_NEVER) return false;
	if (vis == FLUX_SCROLL_ALWAYS) return true;
	return content > viewport + FLUX_SCROLL_VIS_EPS;
}

static FluxScrollViewLayout inline flux_calc_scroll_layout(
  float view_w, float view_h, float content_w, float content_h, float scroll_x, float scroll_y, bool show_h, bool show_v
) {
	FluxScrollViewLayout l;
	memset(&l, 0, sizeof(l));
	l.content_width  = content_w;
	l.content_height = content_h;

	float ew         = view_w;
	float eh         = view_h;
	if (show_v) ew -= FLUX_SCROLLBAR_SIZE;
	if (show_h) eh -= FLUX_SCROLLBAR_SIZE;

	l.v_bar.visible = show_v;
		if (show_v) {
			l.v_bar.bar_rect      = ( FluxRect ) {view_w - FLUX_SCROLLBAR_SIZE, 0, FLUX_SCROLLBAR_SIZE, eh};
			l.v_bar.track_rect    = l.v_bar.bar_rect;
			l.v_bar.track_rect.y += FLUX_SCROLLBAR_PAD;
			l.v_bar.track_rect.h  = flux_maxf(0.0f, l.v_bar.track_rect.h - 2 * FLUX_SCROLLBAR_PAD);

				if (l.v_bar.track_rect.h > 0) {
					float ratio          = eh / content_h;
					float thumb_h        = flux_maxf(FLUX_SCROLLBAR_MIN_THUMB, l.v_bar.track_rect.h * ratio);
					float max_off        = content_h - eh;
					float sr             = max_off > 0 ? scroll_y / max_off : 0.0f;
					float scrollable     = l.v_bar.track_rect.h - thumb_h;
					l.v_bar.thumb_rect.x = l.v_bar.bar_rect.x + FLUX_SCROLLBAR_PAD;
					l.v_bar.thumb_rect.y = l.v_bar.track_rect.y + sr * scrollable;
					l.v_bar.thumb_rect.w = FLUX_SCROLLBAR_SIZE - 2 * FLUX_SCROLLBAR_PAD;
					l.v_bar.thumb_rect.h = thumb_h;
				}
		}

	l.h_bar.visible = show_h;
		if (show_h) {
			l.h_bar.bar_rect      = ( FluxRect ) {0, view_h - FLUX_SCROLLBAR_SIZE, ew, FLUX_SCROLLBAR_SIZE};
			l.h_bar.track_rect    = l.h_bar.bar_rect;
			l.h_bar.track_rect.x += FLUX_SCROLLBAR_PAD;
			l.h_bar.track_rect.w  = flux_maxf(0.0f, l.h_bar.track_rect.w - 2 * FLUX_SCROLLBAR_PAD);

				if (l.h_bar.track_rect.w > 0) {
					float ratio          = ew / content_w;
					float thumb_w        = flux_maxf(FLUX_SCROLLBAR_MIN_THUMB, l.h_bar.track_rect.w * ratio);
					float max_off        = content_w - ew;
					float sr             = max_off > 0 ? scroll_x / max_off : 0.0f;
					float scrollable     = l.h_bar.track_rect.w - thumb_w;
					l.h_bar.thumb_rect.x = l.h_bar.track_rect.x + sr * scrollable;
					l.h_bar.thumb_rect.y = l.h_bar.bar_rect.y + FLUX_SCROLLBAR_PAD;
					l.h_bar.thumb_rect.w = thumb_w;
					l.h_bar.thumb_rect.h = FLUX_SCROLLBAR_SIZE - 2 * FLUX_SCROLLBAR_PAD;
				}
		}

	return l;
}

static void inline flux_draw_elevation_border(
  FluxRenderContext const *rc, FluxRect const *bounds, float radius, bool is_accent
) {
	float     thickness = FLUX_STROKE_WIDTH;
	float     half      = thickness * 0.5f;

	/* Resolve gradient colors from theme */
	FluxColor secondary
	  = is_accent ? (rc->theme ? rc->theme->ctrl_stroke_on_accent_secondary : ft_ctrl_stroke_on_accent_secondary())
	              : (rc->theme ? rc->theme->ctrl_stroke_secondary : ft_ctrl_stroke_secondary());
	FluxColor primary = is_accent
	                    ? (rc->theme ? rc->theme->ctrl_stroke_on_accent_default : ft_ctrl_stroke_on_accent_default())
	                    : (rc->theme ? rc->theme->ctrl_stroke_default : ft_ctrl_stroke_default());

	/* Light mode / accent → band at bottom; dark-mode standard → band at top */
	bool      flip    = !rc->is_dark || is_accent;

	float     band_h  = FLUX_ELEVATION_HEIGHT;
	float     band_start, band_end;
		if (flip) {
			band_start = bounds->y + bounds->h - band_h;
			band_end   = band_start + band_h;
		}
		else {
			band_start = bounds->y;
			band_end   = band_start + band_h;
		}

	/* Build gradient stops — matches WinUI elevation border gradient */
	D2D1_GRADIENT_STOP stops [2];
	stops [0].position                      = FLUX_ELEVATION_STOP1;
	stops [0].color                         = flux_d2d_color(secondary);
	stops [1].position                      = FLUX_ELEVATION_STOP2;
	stops [1].color                         = flux_d2d_color(primary);

	ID2D1GradientStopCollection *collection = NULL;
	ID2D1RenderTarget_CreateGradientStopCollection(
	  FLUX_RT(rc), stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &collection
	);
	if (!collection) return;

	/* Position gradient band and flip direction when needed */
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gp;
		if (flip) {
			gp.startPoint = flux_point(0.0f, band_end);
			gp.endPoint   = flux_point(0.0f, band_start);
		}
		else {
			gp.startPoint = flux_point(0.0f, band_start);
			gp.endPoint   = flux_point(0.0f, band_end);
		}

	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity                     = 1.0f;
	bp.transform._11               = 1;
	bp.transform._12               = 0;
	bp.transform._21               = 0;
	bp.transform._22               = 1;
	bp.transform._31               = 0;
	bp.transform._32               = 0;

	ID2D1LinearGradientBrush *grad = NULL;
	ID2D1RenderTarget_CreateLinearGradientBrush(FLUX_RT(rc), &gp, &bp, collection, &grad);
	ID2D1GradientStopCollection_Release(collection);
	if (!grad) return;

	/* Draw full border with gradient — only the 3-DIP band has
	   visible color; the rest clamps to nearly-transparent. */
	D2D1_ROUNDED_RECT rr;
	rr.rect.left   = bounds->x + half;
	rr.rect.top    = bounds->y + half;
	rr.rect.right  = bounds->x + bounds->w - half;
	rr.rect.bottom = bounds->y + bounds->h - half;
	rr.radiusX     = radius;
	rr.radiusY     = radius;

	ID2D1RenderTarget_DrawRoundedRectangle(FLUX_RT(rc), &rr, ( ID2D1Brush * ) grad, thickness, NULL);

	ID2D1LinearGradientBrush_Release(grad);
}

#endif
