#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"

#include "controls/data/flux_teaching_tip_data.h"
#include "controls/draw/flux_teachtip_internal.h"

#include <string.h>

#define TT_CORNER       8.0f
#define TT_BORDER       1.0f
#define TT_PAD          12.0f
#define TT_TAIL_BAND    8.0f
#define TT_TAIL_HALF    10.0f
#define TT_TAIL_SHORT   10.0f
#define TT_HIGHLIGHT_IN 7.0f
#define TT_TEXT_FONT    14.0f
#define TT_ICON_SIZE    16.0f

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

static bool tt_rec_tail(XentDlBuilder *b, FluxTeachingTipPaintInfo const *info, FluxColor fill, FluxColor border) {
	if (info->tail_edge == FLUX_TIP_TAIL_EDGE_NONE) return true;
	TtTail    t   = tt_tail_points(info);
	XentColor ink = flux_rec_xcolor(border);
	return flux_rec_fill_triangle(b, t.base_a.x, t.base_a.y, t.apex.x, t.apex.y, t.base_b.x, t.base_b.y, fill)
	    && xent_dl_strokeline(b, t.base_a.x, t.base_a.y, t.apex.x, t.apex.y, ink, TT_BORDER, XENT_STROKE_CAP_FLAT)
	    && xent_dl_strokeline(b, t.apex.x, t.apex.y, t.base_b.x, t.base_b.y, ink, TT_BORDER, XENT_STROKE_CAP_FLAT);
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

static bool
tt_rec_icon(XentDlBuilder *b, FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info, FluxRect const *card) {
	if (!info->model->icon_glyph) return true;
	FluxRect icon_box  = {card->x + TT_PAD, card->y + TT_PAD, TT_ICON_SIZE, TT_ICON_SIZE};
	wchar_t  glyph [2] = {( wchar_t ) info->model->icon_glyph, 0};
	return flux_rec_glyph_wc(b, &icon_box, glyph, TT_ICON_SIZE, flux_rec_theme(rc)->text_primary);
}

static XentDl *tt_record(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	FluxContrastColors const *p      = &flux_rec_theme(rc)->contrast;
	FluxColor                 border = p->active ? p->window_text : tt_border_color();
	FluxRect card = {TT_TAIL_BAND, TT_TAIL_BAND, info->tip_w - 2.0f * TT_TAIL_BAND, info->tip_h - 2.0f * TT_TAIL_BAND};
	bool     ok   = flux_rec_fill_rounded(b, &card, TT_CORNER, info->background)
	             && flux_rec_stroke_rounded(b, &card, TT_CORNER - 0.5f, border, TT_BORDER)
	             && tt_rec_tail(b, info, info->background, border);
	ok            = ok && (p->active || tt_rec_highlight(b, info, &card, rc->is_dark));
	ok            = ok && tt_rec_icon(b, rc, info, &card);
	return flux_rec_finish(b, ok);
}

XentDl *flux_teachtip_record_surface(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info) {
	if (!rc || !info) return NULL;
	return tt_record(rc, info);
}
