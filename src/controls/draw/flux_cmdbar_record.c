#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_control_record_internal.h"
#include "controls/data/flux_cmdbar_data.h"
#include "render/flux_icon.h"

#include <math.h>

#define FLUX_REC_BEGIN()                \
	XentDlBuilder *b = xent_dl_begin(); \
	if (!b) return NULL

#define FLUX_REC_END(ok) return flux_rec_finish(b, ok)

#define CMD_MORE_GLYPH   L"\xE712"

typedef struct AbbColors {
	FluxColor fill;
	FluxColor text;
	bool      elevation;
} AbbColors;

static AbbColors abb_checked(FluxThemeColors const *t, FluxControlState const *s) {
	if (!s->enabled) return (AbbColors) {t->accent_disabled, t->text_on_accent_disabled, false};
	if (s->pressed) return (AbbColors) {t->accent_tertiary, t->text_on_accent_secondary, false};
	if (s->hovered) return (AbbColors) {t->accent_secondary, t->text_on_accent_primary, true};
	return (AbbColors) {t->accent_default, t->text_on_accent_primary, true};
}

static AbbColors abb_colors(FluxThemeColors const *t, FluxControlState const *s, bool checked) {
	if (checked) return abb_checked(t, s);
	if (!s->enabled) return (AbbColors) {(FluxColor) {0}, t->text_disabled, false};
	if (s->pressed) return (AbbColors) {t->subtle_fill_tertiary, t->text_secondary, false};
	if (s->hovered) return (AbbColors) {t->subtle_fill_secondary, t->text_primary, false};
	return (AbbColors) {(FluxColor) {0}, t->text_primary, false};
}

static FluxRect abb_inner(FluxRect const *r, uint8_t view) {
	float bottom = view == FLUX_APPBAR_VIEW_COMPACT ? FLUX_ABB_BORDER_COMPACT_BOT : FLUX_ABB_BORDER_V;
	return (FluxRect) {
	  r->x + FLUX_ABB_BORDER_H, r->y + FLUX_ABB_BORDER_V, r->w - 2.0f * FLUX_ABB_BORDER_H,
	  r->h - FLUX_ABB_BORDER_V - bottom};
}

static bool abb_icon(XentDlBuilder *b, FluxAppBarSnapshot const *a, FluxRect const *r, FluxColor c) {
	if (!a->icon) return true;
	bool           right = a->view == FLUX_APPBAR_VIEW_RIGHT;
	float          x     = right ? r->x + FLUX_ABB_ICON_RIGHT_LEFT : r->x + (r->w - FLUX_ABB_ICON) * 0.5f;
	FluxRect       box   = {x, r->y + FLUX_ABB_ICON_TOP, FLUX_ABB_ICON, FLUX_ABB_ICON};
	XentTextStyle  style = {.font_size = FLUX_ABB_ICON, .h_align = 1u, .v_align = 1u, .flags = XENT_DL_TEXT_ICON};
	char           utf8 [8];
	wchar_t const *wc = flux_icon_lookup(a->icon);
	if (wc && flux_icon_to_utf8(wc, utf8, sizeof(utf8)) > 0) return flux_rec_text_ex(b, &box, utf8, &style, c);
	return flux_rec_text_ex(b, &box, a->icon, &style, c);
}

static bool abb_label(XentDlBuilder *b, FluxAppBarSnapshot const *a, FluxRect const *r, FluxColor c) {
	if (!a->label) return true;
	if (a->view == FLUX_APPBAR_VIEW_RIGHT) {
		float    x   = FLUX_ABB_ICON_RIGHT_LEFT + FLUX_ABB_ICON + FLUX_ABB_RIGHT_LABEL_LEFT;
		FluxRect box = {r->x + x, r->y + FLUX_ABB_ICON_TOP, r->w - x - FLUX_ABB_RIGHT_LABEL_RIGHT, FLUX_ABB_LABEL_LINE};
		XentTextStyle style = {.font_size = FLUX_ABB_LABEL_SIZE, .font_weight = 400u, .max_lines = 1u};
		return flux_rec_text_ex(b, &box, a->label, &style, c);
	}
	if (a->view != FLUX_APPBAR_VIEW_FULL) return true;
	FluxRect box = {
	  r->x + FLUX_ABB_LABEL_PAD_H, r->y + FLUX_ABB_ICON_TOP + FLUX_ABB_ICON + FLUX_ABB_ICON_BOTTOM,
	  r->w - 2.0f * FLUX_ABB_LABEL_PAD_H, a->label_h};
	XentTextStyle style
	  = {.font_size = FLUX_ABB_LABEL_SIZE, .font_weight = 400u, .h_align = 1u, .flags = XENT_DL_TEXT_WRAP};
	return flux_rec_text_ex(b, &box, a->label, &style, c);
}

static bool abb_separator(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *r) {
	FluxRect line = {
	  r->x + FLUX_ABB_SEP_PAD_H, r->y + FLUX_ABB_SEP_PAD_V, FLUX_ABB_SEP_W,
	  fmaxf(0.0f, r->h - 2.0f * FLUX_ABB_SEP_PAD_V)};
	return flux_rec_fill_rounded(b, &line, FLUX_ABB_SEP_W * 0.5f, flux_rec_theme(rc)->divider_stroke_default);
}

static bool abb_focus(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, FluxControlState const *s) {
	if (!s->focused) return true;
	FluxRect outer = {r->x - 3.0f, r->y - 3.0f, r->w + 6.0f, r->h + 6.0f};
	return flux_rec_focus_rect(b, t, &outer, FLUX_ABB_CORNER);
}

static bool abb_button(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *r, FluxControlState const *s
) {
	FluxAppBarSnapshot const *a     = &snap->u.app_bar;
	FluxThemeColors const    *t     = flux_rec_theme(rc);
	AbbColors                 c     = abb_colors(t, s, a->checked);
	FluxRect                  inner = abb_inner(r, a->view);
	FluxColor                 fill  = flux_rec_tweenfill(rc, snap, c.fill);
	bool                      ok    = !(fill.rgba & 0xffu) || flux_rec_fill_rounded(b, &inner, FLUX_ABB_CORNER, fill);
	if (ok && c.elevation) ok = flux_rec_elevation_border(b, rc, &inner, FLUX_ABB_CORNER, true);
	return ok && abb_icon(b, a, r, c.text) && abb_label(b, a, r, c.text) && abb_focus(b, t, r, s);
}

XentDl *flux_appbar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (bounds->w <= 0.0f || bounds->h <= 0.0f) return NULL;
	FLUX_REC_BEGIN();
	if (snap->u.app_bar.kind == FLUX_APPBAR_SEPARATOR) FLUX_REC_END(abb_separator(b, rc, bounds));
	FLUX_REC_END(abb_button(b, rc, snap, bounds, state));
}

XentDl *flux_cmdbar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxCmdBarSnapshot const *c = &snap->u.cmd_bar;
	if (!c->open || c->bg_h <= 0.0f) return NULL;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t     = flux_rec_theme(rc);
	FluxRect               bg    = {bounds->x, bounds->y + c->bg_y, bounds->w, c->bg_h};
	FluxRect               inset = {bg.x + 0.5f, bg.y + 0.5f, bg.w - 1.0f, bg.h - 1.0f};
	FLUX_REC_END(
	  flux_rec_fill_rounded(b, &bg, FLUX_CMDBAR_CORNER, t->flyout_background)
	  && flux_rec_stroke_rounded(b, &inset, FLUX_CMDBAR_CORNER, t->card_stroke_solid, 1.0f)
	);
}

XentDl *flux_cmdmore_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (bounds->w <= 0.0f || bounds->h <= 0.0f) return NULL;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t     = flux_rec_theme(rc);
	AbbColors              c     = abb_colors(t, state, false);
	FluxRect               inner = {bounds->x + 2.0f, bounds->y + 6.0f, bounds->w - 8.0f, bounds->h - 12.0f};
	FluxColor              fill  = flux_rec_tweenfill(rc, snap, c.fill);
	float                  cy    = inner.y + inner.h * 0.5f;
	FluxRect               glyph = {inner.x, cy - FLUX_CMDBAR_MORE_GLYPH * 0.5f, inner.w, FLUX_CMDBAR_MORE_GLYPH};
	bool                   ok    = !(fill.rgba & 0xffu) || flux_rec_fill_rounded(b, &inner, FLUX_ABB_CORNER, fill);
	FLUX_REC_END(
	  ok
	  && flux_rec_glyph_wc(b, &glyph, CMD_MORE_GLYPH, FLUX_CMDBAR_MORE_GLYPH, c.text)
	  && abb_focus(b, t, bounds, state)
	);
}
