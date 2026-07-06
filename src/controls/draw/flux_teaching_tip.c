#include "render/flux_fluent.h"
#include "render/flux_icon.h"

#include "fluxent/controls/flux_teaching_tip_data.h"

#include <string.h>

/* WinUI TeachingTip chrome (TeachingTip_themeresources.xaml + TeachingTip.xaml).
 * Card corner 8 (OverlayCornerRadius), 1px SurfaceStrokeColorDefault border,
 * 12px content padding; the popup content is the 5x5 tail-occlusion grid with
 * an 8px tail band on every side. Tail polygon 20x10 with 2px card occlusion
 * (visible 16x8), same fill as the card, stroked only on its outward edges.
 * 1px TeachingTipTopHighlight strip at y=1, inset (radius-1)=7 per corner. */
#define TT_CORNER        8.0f
#define TT_BORDER        1.0f
#define TT_PAD           12.0f
#define TT_TAIL_BAND     8.0f
#define TT_TAIL_HALF     10.0f /* half the 20px long side */
#define TT_TAIL_SHORT    10.0f /* drawn short side; 2px occluded by the card */
#define TT_HIGHLIGHT_IN  7.0f  /* corner inset = radius - 1 */
#define TT_TEXT_FONT     14.0f
#define TT_ICON_SIZE     16.0f
#define TT_X_SIZE        40.0f
#define TT_X_FONT        16.0f
#define TT_BTN_CORNER    4.0f
#define TT_X_GLYPH       L"\xE711"

/* SurfaceStrokeColorDefault: 0x66757575 in both themes. */
static FluxColor tt_border_color(void) { return flux_color_rgba(0x75, 0x75, 0x75, 0x66); }

/* TeachingTipTopHighlightBrush: light 0x99FFFFFF, dark 0x0DFFFFFF. */
static FluxColor tt_highlight_color(bool is_dark) {
	return is_dark ? flux_color_rgba(255, 255, 255, 0x0d) : flux_color_rgba(255, 255, 255, 0x99);
}

static FluxTextStyle tt_text_style(bool title, FluxColor color) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = TT_TEXT_FONT;
	ts.font_weight = title ? FLUX_FONT_SEMI_BOLD : FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_TOP;
	ts.color       = color;
	ts.word_wrap   = true;
	return ts;
}

float flux_teaching_tip_text_height(FluxTextRenderer *text, char const *s, bool title, float max_w) {
	if (!s || !s [0] || max_w <= 0.0f) return 0.0f;
	if (!text) return 20.0f; /* one 14px line box (headless fallback) */
	FluxTextStyle ts = tt_text_style(title, flux_color_rgba(0, 0, 0, 0xff));
	FluxSize      sz = flux_text_measure(text, s, &ts, max_w);
	return sz.h > 0.0f ? sz.h : 20.0f;
}

static void
tt_draw_glyph(FluxRenderContext const *rc, FluxRect const *box, wchar_t const *glyph, float font, FluxColor color) {
	if (!rc->text) return;
	char utf8 [8];
	flux_icon_to_utf8(glyph, utf8, sizeof(utf8));

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = flux_icon_font_family();
	ts.font_size   = font;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = color;
	flux_text_draw(rc->text, FLUX_RT(rc), utf8, box, &ts);
}

/* Tail apex/base points along one card edge; the base sits TT_TAIL_SHORT back
 * from the apex, so it overlaps the card by 2px and the fill overpaints the
 * card border segment beneath the tail (WinUI's occlusion + -1 margin). */
typedef struct TtTail {
	FluxPoint apex;
	FluxPoint base_a;
	FluxPoint base_b;
} TtTail;

static TtTail tt_tail_points(FluxTeachingTipPaintInfo const *info) {
	float  c = info->tail_center;
	TtTail t = {0};
	switch (info->tail_edge) {
	case FLUX_TIP_TAIL_EDGE_TOP:
		t.apex   = (FluxPoint) {c, 0.0f};
		t.base_a = (FluxPoint) {c - TT_TAIL_HALF, TT_TAIL_SHORT};
		t.base_b = (FluxPoint) {c + TT_TAIL_HALF, TT_TAIL_SHORT};
		break;
	case FLUX_TIP_TAIL_EDGE_BOTTOM:
		t.apex   = (FluxPoint) {c, info->tip_h};
		t.base_a = (FluxPoint) {c - TT_TAIL_HALF, info->tip_h - TT_TAIL_SHORT};
		t.base_b = (FluxPoint) {c + TT_TAIL_HALF, info->tip_h - TT_TAIL_SHORT};
		break;
	case FLUX_TIP_TAIL_EDGE_LEFT:
		t.apex   = (FluxPoint) {0.0f, c};
		t.base_a = (FluxPoint) {TT_TAIL_SHORT, c - TT_TAIL_HALF};
		t.base_b = (FluxPoint) {TT_TAIL_SHORT, c + TT_TAIL_HALF};
		break;
	default: /* FLUX_TIP_TAIL_EDGE_RIGHT */
		t.apex   = (FluxPoint) {info->tip_w, c};
		t.base_a = (FluxPoint) {info->tip_w - TT_TAIL_SHORT, c - TT_TAIL_HALF};
		t.base_b = (FluxPoint) {info->tip_w - TT_TAIL_SHORT, c + TT_TAIL_HALF};
		break;
	}
	return t;
}

static void tt_fill_tail(FluxRenderContext const *rc, TtTail const *t, FluxColor fill) {
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
	if (!factory) return;
	ID2D1PathGeometry *geo = NULL;
	ID2D1Factory_CreatePathGeometry(factory, &geo);
	ID2D1Factory_Release(factory);
	if (!geo) return;

	ID2D1GeometrySink *sink = NULL;
	ID2D1PathGeometry_Open(geo, &sink);
	if (!sink) {
		ID2D1PathGeometry_Release(geo);
		return;
	}
	ID2D1GeometrySink_BeginFigure(sink, flux_point(t->base_a.x, t->base_a.y), D2D1_FIGURE_BEGIN_FILLED);
	ID2D1GeometrySink_AddLine(sink, flux_point(t->apex.x, t->apex.y));
	ID2D1GeometrySink_AddLine(sink, flux_point(t->base_b.x, t->base_b.y));
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);

	flux_set_brush(rc, fill);
	ID2D1RenderTarget_FillGeometry(FLUX_RT(rc), ( ID2D1Geometry * ) geo, ( ID2D1Brush * ) rc->brush, NULL);
	ID2D1PathGeometry_Release(geo);
}

/* Fill overpaints the card border beneath the tail base; only the two outward
 * edges are stroked, so tail + card read as one closed outline (WinUI omits
 * the card's per-edge border under the tail the same way). */
static void tt_draw_tail(FluxRenderContext const *rc, FluxTeachingTipPaintInfo const *info, FluxColor fill) {
	if (info->tail_edge == FLUX_TIP_TAIL_EDGE_NONE) return;
	TtTail t = tt_tail_points(info);
	tt_fill_tail(rc, &t, fill);
	FluxLineSpec a = {t.base_a.x, t.base_a.y, t.apex.x, t.apex.y};
	FluxLineSpec b = {t.apex.x, t.apex.y, t.base_b.x, t.base_b.y};
	flux_draw_line(rc, &a, tt_border_color(), TT_BORDER);
	flux_draw_line(rc, &b, tt_border_color(), TT_BORDER);
}

/* 1px top highlight just inside the border, inset (radius-1) from each top
 * corner; split into two segments around the tail when the tail sits on the
 * card's top edge (Bottom-family placements). */
static void tt_draw_highlight(FluxRenderContext const *rc, FluxTeachingTipPaintInfo const *info, FluxRect const *card) {
	FluxColor hl = tt_highlight_color(rc->is_dark);
	float     x0 = card->x + TT_HIGHLIGHT_IN;
	float     x1 = card->x + card->w - TT_HIGHLIGHT_IN;
	float     y  = card->y + TT_BORDER;
	if (x1 <= x0) return;

	if (info->tail_edge != FLUX_TIP_TAIL_EDGE_TOP) {
		flux_fill_rect(rc, &(FluxRect) {x0, y, x1 - x0, 1.0f}, hl);
		return;
	}
	float gap0 = info->tail_center - TT_HIGHLIGHT_IN;
	float gap1 = info->tail_center + TT_HIGHLIGHT_IN;
	if (gap0 > x0) flux_fill_rect(rc, &(FluxRect) {x0, y, gap0 - x0, 1.0f}, hl);
	if (x1 > gap1) flux_fill_rect(rc, &(FluxRect) {gap1, y, x1 - gap1, 1.0f}, hl);
}

/* Footer buttons follow DefaultButtonStyle / AccentButtonStyle state fills. */
static void tt_draw_footer_button(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *r, char const *label, bool accent, bool hot,
  bool pressed
) {
	FluxColor fill, fg;
	if (accent) {
		fill = pressed ? t->accent_tertiary : (hot ? t->accent_secondary : t->accent_default);
		fg   = pressed ? t->text_on_accent_secondary : t->text_on_accent_primary;
	}
	else {
		fill = pressed ? t->ctrl_fill_tertiary : (hot ? t->ctrl_fill_secondary : t->ctrl_fill_default);
		fg   = pressed ? t->text_secondary : t->text_primary;
	}
	flux_fill_rounded_rect(rc, r, TT_BTN_CORNER, fill);
	if (!accent) {
		float    half  = TT_BORDER * 0.5f;
		FluxRect inset = {r->x + half, r->y + half, r->w - half * 2.0f, r->h - half * 2.0f};
		flux_draw_rounded_rect(rc, &inset, TT_BTN_CORNER, t->ctrl_stroke_default, TT_BORDER);
	}

	if (!label || !label [0] || !rc->text) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = TT_TEXT_FONT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = fg;
	flux_text_draw(rc->text, FLUX_RT(rc), label, r, &ts);
}

/* Header "X" close button: 40x40, subtle-fill states, 16px ChromeClose glyph. */
static void
tt_draw_x_button(FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *r, bool hot, bool pressed) {
	if (pressed) flux_fill_rounded_rect(rc, r, TT_BTN_CORNER, t->subtle_fill_tertiary);
	else if (hot) flux_fill_rounded_rect(rc, r, TT_BTN_CORNER, t->subtle_fill_secondary);
	tt_draw_glyph(rc, r, TT_X_GLYPH, TT_X_FONT, pressed ? t->text_secondary : t->text_primary);
}

static void tt_draw_texts(FluxRenderContext const *rc, FluxTeachingTipPaintInfo const *info, FluxRect const *card) {
	FluxThemeColors const     *t = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxTeachingTipData const *m = info->model;

	float text_x = card->x + TT_PAD;
	if (m->icon_glyph) {
		FluxRect icon_box = {text_x, card->y + TT_PAD, TT_ICON_SIZE, TT_ICON_SIZE};
		wchar_t  glyph [2] = {( wchar_t ) m->icon_glyph, 0};
		tt_draw_glyph(rc, &icon_box, glyph, TT_ICON_SIZE, t->text_primary);
		text_x += TT_ICON_SIZE + 12.0f; /* icon presenter margin 0,0,12,0 */
	}

	/* Title stack reserves 28px on the right while the header X is shown
	 * (12px pad + 28 = the 40px button column). */
	float text_w = card->x + card->w - TT_PAD - text_x - (info->x_rect.w > 0.0f ? 28.0f : 0.0f);
	if (text_w <= 0.0f || !rc->text) return;

	float y = card->y + TT_PAD;
	if (m->title && m->title [0]) {
		FluxTextStyle ts = tt_text_style(true, t->text_primary);
		float         h  = flux_teaching_tip_text_height(rc->text, m->title, true, text_w);
		FluxRect      r  = {text_x, y, text_w, h};
		flux_text_draw(rc->text, FLUX_RT(rc), m->title, &r, &ts);
		y += h;
	}
	if (m->subtitle && m->subtitle [0]) {
		FluxTextStyle ts = tt_text_style(false, t->text_primary);
		float         h  = flux_teaching_tip_text_height(rc->text, m->subtitle, false, text_w);
		FluxRect      r  = {text_x, y, text_w, h};
		flux_text_draw(rc->text, FLUX_RT(rc), m->subtitle, &r, &ts);
	}
}

void flux_teaching_tip_paint_surface(FluxRenderContext const *rc, FluxTeachingTipPaintInfo const *info) {
	FluxThemeColors const *t    = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               card = {
	  TT_TAIL_BAND, TT_TAIL_BAND, info->tip_w - 2.0f * TT_TAIL_BAND, info->tip_h - 2.0f * TT_TAIL_BAND};

	/* Expand/contract scales the whole surface (card + tail) about the tail
	 * tip, so the tip grows out of / shrinks into its pointer. */
	D2D1_MATRIX_3X2_F prev;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &prev);
	D2D1_MATRIX_3X2_F xform = prev;
	xform._11               = prev._11 * info->scale_x;
	xform._22               = prev._22 * info->scale_y;
	xform._31              += info->scale_cx * (1.0f - info->scale_x) * prev._11;
	xform._32              += info->scale_cy * (1.0f - info->scale_y) * prev._22;
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &xform);

	flux_fill_rounded_rect(rc, &card, TT_CORNER, info->background);
	float    half  = TT_BORDER * 0.5f;
	FluxRect inset = {card.x + half, card.y + half, card.w - half * 2.0f, card.h - half * 2.0f};
	flux_draw_rounded_rect(rc, &inset, TT_CORNER, tt_border_color(), TT_BORDER);

	tt_draw_tail(rc, info, info->background);
	tt_draw_highlight(rc, info, &card);
	tt_draw_texts(rc, info, &card);

	if (info->action_rect.w > 0.0f)
		tt_draw_footer_button(
		  rc, t, &info->action_rect, info->model->action_text, info->model->action_accent,
		  info->hot_zone == FLUX_TIP_ZONE_ACTION, info->pressed_zone == FLUX_TIP_ZONE_ACTION
		);
	if (info->close_rect.w > 0.0f)
		tt_draw_footer_button(
		  rc, t, &info->close_rect, info->model->close_text, false, info->hot_zone == FLUX_TIP_ZONE_CLOSE,
		  info->pressed_zone == FLUX_TIP_ZONE_CLOSE
		);
	if (info->x_rect.w > 0.0f)
		tt_draw_x_button(
		  rc, t, &info->x_rect, info->hot_zone == FLUX_TIP_ZONE_X, info->pressed_zone == FLUX_TIP_ZONE_X
		);

	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &prev);
}

void flux_draw_teaching_tip(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	/* The TeachingTip's owner-tree node is a zero-size focus/anchor stub; the
	 * tip surface paints in its own FluxPopup via flux_teaching_tip_paint_surface. */
	( void ) rc;
	( void ) snap;
	( void ) bounds;
	( void ) state;
}
