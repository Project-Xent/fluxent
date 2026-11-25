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
	FluxColor stroke;
} AbbColors;

static AbbColors abb_checked(FluxThemeColors const *t, FluxControlState const *s) {
	if (!s->enabled) return (AbbColors) {.fill = t->accent_disabled, .text = t->text_on_accent_disabled};
	if (s->pressed) return (AbbColors) {.fill = t->accent_tertiary, .text = t->text_on_accent_secondary};
	if (s->hovered)
		return (AbbColors) {.fill = t->accent_secondary, .text = t->text_on_accent_primary, .elevation = true};
	return (AbbColors) {.fill = t->accent_default, .text = t->text_on_accent_primary, .elevation = true};
}

static AbbColors abb_contrast_toggle(FluxContrastColors const *p, FluxControlState const *s, bool checked) {
	bool hot = s->hovered || s->pressed;
	if (checked) {
		if (!s->enabled) return (AbbColors) {.fill = p->gray_text, .text = p->window, .stroke = p->gray_text};
		if (hot) return (AbbColors) {.fill = p->highlight_text, .text = p->highlight, .stroke = p->highlight};
		return (AbbColors) {.fill = p->highlight, .text = p->highlight_text, .stroke = p->highlight};
	}
	if (!s->enabled) return (AbbColors) {.fill = p->window, .text = p->gray_text, .stroke = p->gray_text};
	if (hot) return (AbbColors) {.fill = p->highlight, .text = p->highlight_text, .stroke = p->button_text};
	return (AbbColors) {.fill = p->button_face, .text = p->button_text, .stroke = p->button_text};
}

static AbbColors abb_contrast_button(FluxContrastColors const *p, FluxControlState const *s) {
	if (!s->enabled) return (AbbColors) {.text = p->gray_text};
	if (s->hovered || s->pressed) return (AbbColors) {.fill = p->highlight, .text = p->highlight_text};
	return (AbbColors) {.text = p->button_text};
}

static AbbColors abb_colors(FluxThemeColors const *t, FluxControlState const *s, bool checked) {
	if (checked) return abb_checked(t, s);
	if (!s->enabled) return (AbbColors) {.text = t->text_disabled};
	if (s->pressed) return (AbbColors) {.fill = t->subtle_fill_tertiary, .text = t->text_secondary};
	if (s->hovered) return (AbbColors) {.fill = t->subtle_fill_secondary, .text = t->text_primary};
	return (AbbColors) {.text = t->text_primary};
}

static AbbColors abb_item_colors(FluxThemeColors const *t, FluxControlState const *s, FluxAppBarSnapshot const *a) {
	if (!t->contrast.active) return abb_colors(t, s, a->checked);
	if (a->kind == FLUX_APPBAR_TOGGLE) return abb_contrast_toggle(&t->contrast, s, a->checked);
	return abb_contrast_button(&t->contrast, s);
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
	FluxThemeColors const *t     = flux_rec_theme(rc);
	FluxColor              color = t->contrast.active ? t->contrast.button_text : t->divider_stroke_default;
	return flux_rec_fill_rounded(b, &line, FLUX_ABB_SEP_W * 0.5f, color);
}

static bool abb_focus(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, FluxControlState const *s) {
	if (!s->focused) return true;
	if (t->contrast.active) return flux_rec_focus_rect(b, t, r, FLUX_ABB_CORNER);
	FluxRect outer = {r->x - 3.0f, r->y - 3.0f, r->w + 6.0f, r->h + 6.0f};
	return flux_rec_focus_rect(b, t, &outer, FLUX_ABB_CORNER);
}

static bool abb_button(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *r, FluxControlState const *s
) {
	FluxAppBarSnapshot const *a     = &snap->u.app_bar;
	FluxThemeColors const    *t     = flux_rec_theme(rc);
	AbbColors                 c     = abb_item_colors(t, s, a);
	FluxRect                  inner = abb_inner(r, a->view);
	FluxColor                 fill  = flux_rec_tweenfill(rc, snap, c.fill);
	bool                      ok    = !(fill.rgba & 0xffu) || flux_rec_fill_rounded(b, &inner, FLUX_ABB_CORNER, fill);
	if (ok && c.elevation) ok = flux_rec_elevation_border(b, rc, &inner, FLUX_ABB_CORNER, true);
	ok                    = ok && flux_rec_stroke_rounded(b, &inner, FLUX_ABB_CORNER - 0.5f, c.stroke, 1.0f);
	FluxRect const *focus = t->contrast.active ? &inner : r;
	return ok && abb_icon(b, a, r, c.text) && abb_label(b, a, r, c.text) && abb_focus(b, t, focus, s);
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
	FluxThemeColors const    *t = flux_rec_theme(rc);
	if ((!c->open && !t->contrast.active) || c->bg_h <= 0.0f) return NULL;
	FLUX_REC_BEGIN();
	FluxRect bg = {bounds->x, bounds->y + c->bg_y, bounds->w, c->bg_h};
	if (t->contrast.active) FLUX_REC_END(flux_rec_fill_rounded(b, &bg, FLUX_CMDBAR_CORNER, t->contrast.button_face));
	FluxRect inset = {bg.x + 0.5f, bg.y + 0.5f, bg.w - 1.0f, bg.h - 1.0f};
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
	FluxThemeColors const *t = flux_rec_theme(rc);
	AbbColors c     = t->contrast.active ? abb_contrast_button(&t->contrast, state) : abb_colors(t, state, false);
	FluxRect  inner = {bounds->x + 2.0f, bounds->y + 6.0f, bounds->w - 8.0f, bounds->h - 12.0f};
	FluxColor fill  = flux_rec_tweenfill(rc, snap, c.fill);
	float     cy    = inner.y + inner.h * 0.5f;
	FluxRect  glyph = {inner.x, cy - FLUX_CMDBAR_MORE_GLYPH * 0.5f, inner.w, FLUX_CMDBAR_MORE_GLYPH};
	bool      ok    = !(fill.rgba & 0xffu) || flux_rec_fill_rounded(b, &inner, FLUX_ABB_CORNER, fill);
	FLUX_REC_END(
	  ok
	  && flux_rec_glyph_wc(b, &glyph, CMD_MORE_GLYPH, FLUX_CMDBAR_MORE_GLYPH, c.text)
	  && abb_focus(b, t, t->contrast.active ? &inner : bounds, state)
	);
}
