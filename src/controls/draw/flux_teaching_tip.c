#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"

#include "controls/data/flux_teaching_tip_data.h"
#include "controls/draw/flux_teachtip_internal.h"

#include <string.h>

/* WinUI TeachingTip chrome → XentDl (record/replay). */
#define TT_CORNER       8.0f
#define TT_BORDER       1.0f
#define TT_PAD          12.0f
#define TT_TAIL_BAND    8.0f
#define TT_TAIL_HALF    10.0f
#define TT_TAIL_SHORT   10.0f
#define TT_HIGHLIGHT_IN 7.0f
#define TT_TEXT_FONT    14.0f
#define TT_ICON_SIZE    16.0f
#define TT_X_SIZE       40.0f
#define TT_X_FONT       16.0f
#define TT_BTN_CORNER   4.0f
#define TT_X_GLYPH      L"\xE711"

static FluxColor tt_border_color(void) { return flux_color_rgba(0x75, 0x75, 0x75, 0x66); }

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

float flux_teachtip_text_height(FluxText *text, char const *s, bool title, float max_w) {
	if (!s || !s [0] || max_w <= 0.0f) return 0.0f;
	if (!text) return 20.0f;
	FluxTextStyle ts = tt_text_style(title, flux_color_rgba(0, 0, 0, 0xff));
	FluxSize      sz = flux_text_measure(text, s, &ts, max_w);
	return sz.h > 0.0f ? sz.h : 20.0f;
}

typedef struct TtTail {
	FluxPoint apex;
	FluxPoint base_a;
	FluxPoint base_b;
} TtTail;

static TtTail tt_tail_points(FluxTeachingTipPaintInfo const *info) {
	float  c = info->tail_center;
	TtTail t = {0};
	switch (info->tail_edge) {
	case FLUX_TIP_TAIL_EDGE_TOP :
		t.apex   = (FluxPoint) {c, 0.0f};
		t.base_a = (FluxPoint) {c - TT_TAIL_HALF, TT_TAIL_SHORT};
		t.base_b = (FluxPoint) {c + TT_TAIL_HALF, TT_TAIL_SHORT};
		break;
	case FLUX_TIP_TAIL_EDGE_BOTTOM :
		t.apex   = (FluxPoint) {c, info->tip_h};
		t.base_a = (FluxPoint) {c - TT_TAIL_HALF, info->tip_h - TT_TAIL_SHORT};
		t.base_b = (FluxPoint) {c + TT_TAIL_HALF, info->tip_h - TT_TAIL_SHORT};
		break;
	case FLUX_TIP_TAIL_EDGE_LEFT :
		t.apex   = (FluxPoint) {0.0f, c};
		t.base_a = (FluxPoint) {TT_TAIL_SHORT, c - TT_TAIL_HALF};
		t.base_b = (FluxPoint) {TT_TAIL_SHORT, c + TT_TAIL_HALF};
		break;
	default :
		t.apex   = (FluxPoint) {info->tip_w, c};
		t.base_a = (FluxPoint) {info->tip_w - TT_TAIL_SHORT, c - TT_TAIL_HALF};
		t.base_b = (FluxPoint) {info->tip_w - TT_TAIL_SHORT, c + TT_TAIL_HALF};
		break;
	}
	return t;
}

static bool tt_rec_tail(XentDlBuilder *b, FluxTeachingTipPaintInfo const *info, FluxColor fill) {
	if (info->tail_edge == FLUX_TIP_TAIL_EDGE_NONE) return true;
	TtTail t  = tt_tail_points(info);
	bool   ok = flux_rec_fill_triangle(b, t.base_a.x, t.base_a.y, t.apex.x, t.apex.y, t.base_b.x, t.base_b.y, fill);
	if (ok)
		ok = xent_dl_strokeline(
		  b, t.base_a.x, t.base_a.y, t.apex.x, t.apex.y, flux_rec_xcolor(tt_border_color()), TT_BORDER,
		  XENT_STROKE_CAP_FLAT
		);
	if (ok)
		ok = xent_dl_strokeline(
		  b, t.apex.x, t.apex.y, t.base_b.x, t.base_b.y, flux_rec_xcolor(tt_border_color()), TT_BORDER,
		  XENT_STROKE_CAP_FLAT
		);
	return ok;
}

static bool tt_rec_highlight(XentDlBuilder *b, FluxTeachingTipPaintInfo const *info, FluxRect const *card, bool dark) {
	FluxColor hl = tt_highlight_color(dark);
	float     x0 = card->x + TT_HIGHLIGHT_IN;
	float     x1 = card->x + card->w - TT_HIGHLIGHT_IN;
	float     y  = card->y + TT_BORDER;
	if (x1 <= x0) return true;
	if (info->tail_edge != FLUX_TIP_TAIL_EDGE_TOP) {
		FluxRect r = {x0, y, x1 - x0, 1.0f};
		return flux_rec_fill_rect(b, &r, hl);
	}
	float gap0 = info->tail_center - TT_HIGHLIGHT_IN;
	float gap1 = info->tail_center + TT_HIGHLIGHT_IN;
	bool  ok   = true;
	if (gap0 > x0) {
		FluxRect r = {x0, y, gap0 - x0, 1.0f};
		ok         = flux_rec_fill_rect(b, &r, hl);
	}
	if (ok && x1 > gap1) {
		FluxRect r = {gap1, y, x1 - gap1, 1.0f};
		ok         = flux_rec_fill_rect(b, &r, hl);
	}
	return ok;
}

static bool tt_rec_footer_button(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, char const *label, bool accent, bool hot, bool pressed
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
	bool ok = flux_rec_fill_rounded(b, r, TT_BTN_CORNER, fill);
	if (ok && !accent) {
		float    half  = TT_BORDER * 0.5f;
		FluxRect inset = {r->x + half, r->y + half, r->w - half * 2.0f, r->h - half * 2.0f};
		ok             = flux_rec_stroke_rounded(b, &inset, TT_BTN_CORNER, t->ctrl_stroke_default, TT_BORDER);
	}
	if (!ok || !label || !label [0]) return ok;
	XentTextStyle style = {.font_size = TT_TEXT_FONT, .font_weight = FLUX_FONT_REGULAR, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, r, label, &style, fg);
}

static bool tt_rec_x_button(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, bool hot, bool pressed) {
	bool ok = true;
	if (pressed) ok = flux_rec_fill_rounded(b, r, TT_BTN_CORNER, t->subtle_fill_tertiary);
	else if (hot) ok = flux_rec_fill_rounded(b, r, TT_BTN_CORNER, t->subtle_fill_secondary);
	return ok && flux_rec_glyph_wc(b, r, TT_X_GLYPH, TT_X_FONT, pressed ? t->text_secondary : t->text_primary);
}

static bool
tt_rec_texts(XentDlBuilder *b, FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info, FluxRect const *card) {
	FluxThemeColors const     *t      = flux_rec_theme(rc);
	FluxTeachingTipData const *m      = info->model;
	float                      text_x = card->x + TT_PAD;
	bool                       ok     = true;
	if (m->icon_glyph) {
		FluxRect icon_box   = {text_x, card->y + TT_PAD, TT_ICON_SIZE, TT_ICON_SIZE};
		wchar_t  glyph [2]  = {( wchar_t ) m->icon_glyph, 0};
		ok                  = flux_rec_glyph_wc(b, &icon_box, glyph, TT_ICON_SIZE, t->text_primary);
		text_x             += TT_ICON_SIZE + 12.0f;
	}
	float text_w = card->x + card->w - TT_PAD - text_x - (info->x_rect.w > 0.0f ? 28.0f : 0.0f);
	if (!ok || text_w <= 0.0f || !rc->text) return ok;

	float y = card->y + TT_PAD;
	if (m->title && m->title [0]) {
		float         h = flux_teachtip_text_height(rc->text, m->title, true, text_w);
		FluxRect      r = {text_x, y, text_w, h};
		XentTextStyle style
		  = {.font_size = TT_TEXT_FONT, .font_weight = FLUX_FONT_SEMI_BOLD, .flags = XENT_DL_TEXT_WRAP};
		ok  = flux_rec_text_ex(b, &r, m->title, &style, t->text_primary);
		y  += h;
	}
	if (ok && m->subtitle && m->subtitle [0]) {
		float         h     = flux_teachtip_text_height(rc->text, m->subtitle, false, text_w);
		FluxRect      r     = {text_x, y, text_w, h};
		XentTextStyle style = {.font_size = TT_TEXT_FONT, .font_weight = FLUX_FONT_REGULAR, .flags = XENT_DL_TEXT_WRAP};
		ok                  = flux_rec_text_ex(b, &r, m->subtitle, &style, t->text_primary);
	}
	return ok;
}

static XentDl *tt_record(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	FluxThemeColors const *t = flux_rec_theme(rc);
	FluxRect card = {TT_TAIL_BAND, TT_TAIL_BAND, info->tip_w - 2.0f * TT_TAIL_BAND, info->tip_h - 2.0f * TT_TAIL_BAND};

	/* Scale about tail tip: x'=cx+(x-cx)*sx → m00=sx, m02=cx*(1-sx); same for y. */
	XentAffine2 scale = {
	  info->scale_x, 0.0f, info->scale_cx * (1.0f - info->scale_x), 0.0f, info->scale_y,
	  info->scale_cy * (1.0f - info->scale_y)};
	bool ok = xent_dl_pushtransform(b, scale);
	if (ok) ok = flux_rec_fill_rounded(b, &card, TT_CORNER, info->background);
	if (ok) {
		float    half  = TT_BORDER * 0.5f;
		FluxRect inset = {card.x + half, card.y + half, card.w - half * 2.0f, card.h - half * 2.0f};
		ok             = flux_rec_stroke_rounded(b, &inset, TT_CORNER, tt_border_color(), TT_BORDER);
	}
	if (ok) ok = tt_rec_tail(b, info, info->background);
	if (ok) ok = tt_rec_highlight(b, info, &card, rc->is_dark);
	if (ok) ok = tt_rec_texts(b, rc, info, &card);
	if (ok && info->action_rect.w > 0.0f)
		ok = tt_rec_footer_button(
		  b, t, &info->action_rect, info->model->action_text, info->model->action_accent,
		  info->hot_zone == FLUX_TIP_ZONE_ACTION, info->pressed_zone == FLUX_TIP_ZONE_ACTION
		);
	if (ok && info->close_rect.w > 0.0f)
		ok = tt_rec_footer_button(
		  b, t, &info->close_rect, info->model->close_text, false, info->hot_zone == FLUX_TIP_ZONE_CLOSE,
		  info->pressed_zone == FLUX_TIP_ZONE_CLOSE
		);
	if (ok && info->x_rect.w > 0.0f)
		ok = tt_rec_x_button(
		  b, t, &info->x_rect, info->hot_zone == FLUX_TIP_ZONE_X, info->pressed_zone == FLUX_TIP_ZONE_X
		);
	ok = xent_dl_poptransform(b) && ok;
	return flux_rec_finish(b, ok);
}

XentDl *flux_teachtip_record_surface(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info) {
	if (!rc || !info) return NULL;
	return tt_record(rc, info);
}
