#include "controls/draw/flux_teachtip_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_control_record_internal.h"
#include "fluxent/flux_text.h"
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "render/flux_resource_intern.h"
#include "text/flux_utf.h"

#include <string.h>

#define FLUX_REC_BEGIN()                \
	XentDlBuilder *b = xent_dl_begin(); \
	if (!b) return NULL

#define FLUX_REC_END(ok) return flux_rec_finish(b, ok)

XentDl *flux_container_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *theme = flux_rec_theme(rc);
	if (snap->dialog_surface == FLUX_DIALOG_SURFACE_SMOKE && theme->contrast.active) {
		FluxColor smoke = {(theme->contrast.window.rgba & 0xffffff00u) | 0xccu};
		FLUX_REC_END(flux_rec_fill_rect(b, bounds, smoke));
	}
	bool ok = flux_rec_fill_bg(b, snap, bounds) && flux_rec_stroke_border(b, snap, bounds);
	FLUX_REC_END(ok);
}

static FluxTextStyle text_hit_style(FluxRenderSnap const *snap) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = snap->u.text.font_family;
	ts.font_size   = snap->font_size > 0.0f ? snap->font_size : 14.0f;
	ts.font_weight = snap->u.text.font_weight > 0 ? snap->u.text.font_weight : FLUX_FONT_REGULAR;
	ts.text_align  = snap->u.text.text_alignment;
	ts.vert_align  = snap->u.text.text_vert_alignment;
	ts.word_wrap   = snap->u.text.word_wrap;
	ts.rtl         = snap->rtl;
	ts.trim        = snap->u.text.trim;
	ts.max_lines   = snap->u.text.max_lines;
	return ts;
}

static bool text_draw_selection(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, char const *content
) {
	if (!snap->u.text.selectable || !rc || !rc->text) return true;
	if (snap->u.text.selection_start == snap->u.text.selection_end) return true;

	uint32_t               a   = snap->u.text.selection_start;
	uint32_t               e   = snap->u.text.selection_end;
	uint32_t               lo  = a < e ? a : e;
	uint32_t               hi  = a > e ? a : e;
	uint32_t               len = ( uint32_t ) strlen(content);

	FluxTextStyle          ts  = text_hit_style(snap);
	FluxRect               rects [16];
	FluxTextSelectionQuery q = {
	  {rc->text, content, &ts, bounds->w},
      ( int ) flux_utf8_to_utf16_offset(content, len, lo),
	  ( int ) flux_utf8_to_utf16_offset(content, len, hi), rects, 16
    };
	uint32_t  n   = flux_text_selection(&q);
	FluxColor sel = flux_rec_theme(rc)->accent_default;
	sel.rgba      = (sel.rgba & 0xffffff00u) | 0x66u;
	for (uint32_t i = 0; i < n; i++) {
		float    x  = snap->rtl ? bounds->w - rects [i].x - rects [i].w : rects [i].x;
		FluxRect sr = {bounds->x + x, bounds->y + rects [i].y, rects [i].w, rects [i].h};
		if (!flux_rec_fill_rect(b, &sr, sel)) return false;
	}
	return true;
}

static FluxColor text_item_contrast_color(FluxThemeColors const *theme, FluxItemTextColor kind) {
	switch (kind) {
	case FLUX_ITEM_TEXT_BUTTON    : return theme->contrast.button_text;
	case FLUX_ITEM_TEXT_WINDOW    : return theme->contrast.window_text;
	case FLUX_ITEM_TEXT_HIGHLIGHT : return theme->contrast.highlight_text;
	case FLUX_ITEM_TEXT_DISABLED  : return theme->contrast.gray_text;
	default                       : return theme->text_primary;
	}
}

static FluxColor text_record_color(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (state && !state->enabled) return t->text_disabled;
	if (flux_color_af(snap->u.text.text_color) > 0.0f) return snap->u.text.text_color;
	return t->contrast.active ? text_item_contrast_color(t, snap->u.text.item_color) : t->text_primary;
}

static XentTextStyle text_record_style(FluxRenderSnap const *snap) {
	return (XentTextStyle) {
	  .font_size   = snap->font_size > 0.0f ? snap->font_size : 14.0f,
	  .font_weight = snap->u.text.font_weight > 0 ? ( uint16_t ) snap->u.text.font_weight : 400u,
	  .h_align     = ( uint8_t ) snap->u.text.text_alignment,
	  .v_align     = ( uint8_t ) snap->u.text.text_vert_alignment,
	  .flags       = snap->u.text.word_wrap ? XENT_DL_TEXT_WRAP : 0u,
	  .max_lines   = snap->u.text.max_lines,
	  .trim        = snap->u.text.trim,
	  .font_family = snap->u.text.font_family,
	};
}

XentDl *flux_text_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	bool        ok      = flux_rec_fill_bg(b, snap, bounds) && flux_rec_stroke_border(b, snap, bounds);
	char const *content = snap->u.text.text_content;
	if (ok && content && content [0]) {
		XentTextStyle style = text_record_style(snap);
		ok                  = text_draw_selection(b, rc, snap, bounds, content);
		if (ok) ok = flux_rec_text_ex(b, bounds, content, &style, text_record_color(rc, snap, state));
	}
	FLUX_REC_END(ok);
}

XentDl *flux_divider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t     = flux_rec_theme(rc);
	FluxColor              color = t->divider_stroke_default;
	bool                   ok;
	if (bounds->w > bounds->h) {
		float y = bounds->y + bounds->h * 0.5f;
		ok      = flux_rec_line_h(b, bounds->x, bounds->x + bounds->w, y, 1.0f, color);
	}
	else {
		FluxRect line = {bounds->x + bounds->w * 0.5f, bounds->y, 1.0f, bounds->h};
		ok            = flux_rec_fill_rect(b, &line, color);
	}
	FLUX_REC_END(ok);
}

XentDl *flux_card_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t      = flux_rec_theme(rc);
	FluxRect               sb     = flux_rec_snap_bounds(rc, bounds);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : 8.0f;
	float                  width  = flux_rec_hairline(rc);
	if (snap->dialog_surface == FLUX_DIALOG_SURFACE_CARD && t->contrast.active) width = 2.0f * width;
	FluxColor bg = snap->background.rgba ? snap->background : t->card_bg_default;
	if (snap->dialog_surface == FLUX_DIALOG_SURFACE_CARD) bg = t->solid_background;
	bool ok = flux_rec_inner_border_rounded(b, &sb, radius, width, bg, t->card_stroke_default);
	FLUX_REC_END(ok);
}

static FluxColor hyperlink_text_color(
  FluxThemeColors const *t, FluxRenderSnap const *snap, FluxControlState const *state, FluxRecStateAnim anim
) {
	if (state && !state->enabled) return t->text_disabled;
	if (t->contrast.active) {
		if (state && state->pressed) return t->contrast.highlight;
		if (state && state->hovered) return t->contrast.window_text;
		return t->contrast.hotlight;
	}
	if (flux_color_af(snap->u.button.text_color) > 0.0f) return snap->u.button.text_color;
	return flux_anim_lerp_color(
	  flux_anim_lerp_color(t->accent_default, t->accent_secondary, anim.hover), t->accent_tertiary, anim.press
	);
}

static bool hyperlink_hover_bg(
  XentDlBuilder *b, FluxRect const *bounds, float radius, FluxControlState const *state, FluxThemeColors const *t,
  FluxRecStateAnim anim
) {
	if (t->contrast.active) return flux_rec_fill_rounded(b, bounds, radius, t->contrast.window);
	if (!state || !state->enabled) return true;
	float bg_t = anim.hover > anim.press ? anim.hover : anim.press;
	if (bg_t <= 0.001f) return true;
	FluxColor transparent = flux_color_rgba(0, 0, 0, 0);
	FluxColor hover_bg    = t->subtle_fill_secondary;
	FluxColor press_bg    = t->subtle_fill_tertiary;
	FluxColor bg = flux_anim_lerp_color(flux_anim_lerp_color(transparent, hover_bg, anim.hover), press_bg, anim.press);
	return flux_rec_fill_rounded(b, bounds, radius, bg);
}

XentDl *flux_link_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	char const *label = snap->u.button.label ? snap->u.button.label : snap->u.button.text_content;
	if (!label || !label [0]) return NULL;

	FLUX_REC_BEGIN();
	FluxRect               sb         = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t          = flux_rec_theme(rc);
	float                  font_size  = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	float                  radius     = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxRecStateAnim       anim       = flux_rec_animstate(rc, snap, state, false);
	FluxColor              text_color = hyperlink_text_color(t, snap, state, anim);
	bool                   ok         = hyperlink_hover_bg(b, &sb, radius, state, t, anim);
	if (ok && state && state->focused && state->enabled) ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &sb, radius);

	FluxRect text_bounds = {
	  sb.x + FLUX_BTN_PAD_LEFT, sb.y + FLUX_BTN_PAD_TOP, flux_maxf(0.0f, sb.w - FLUX_BTN_PAD_LEFT - FLUX_BTN_PAD_RIGHT),
	  flux_maxf(0.0f, sb.h - FLUX_BTN_PAD_TOP - FLUX_BTN_PAD_BOTTOM)};
	XentTextStyle style
	  = {.font_size = font_size, .font_weight = 400u, .h_align = 1u, .v_align = 1u, .font_family = NULL};
	if (ok) ok = flux_rec_text_ex(b, &text_bounds, label, &style, text_color);
	if (ok && state && state->enabled && rc && rc->text) {
		FluxTextStyle ms;
		memset(&ms, 0, sizeof(ms));
		ms.font_size   = font_size;
		ms.font_weight = style.font_weight;
		ms.font_family = style.font_family;
		FluxSize sz    = flux_text_measure(rc->text, label, &ms, 0.0f);
		float    x0    = text_bounds.x + (text_bounds.w - sz.w) * 0.5f;
		float    y     = flux_rec_snapy(rc, sb.y + (sb.h + sz.h) * 0.5f);
		ok = xent_dl_strokeline(b, x0, y, x0 + sz.w, y, flux_rec_xcolor(text_color), 1.0f, XENT_STROKE_CAP_FLAT);
	}
	FLUX_REC_END(ok);
}

XentDl *flux_button_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_button(b, rc, snap, bounds, state, false));
}

XentDl *flux_toggle_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_button(b, rc, snap, bounds, state, snap->u.button.is_checked));
}

XentDl *flux_repeat_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	return flux_button_record(rc, snap, bounds, state);
}

XentDl *flux_checkbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_checkbox(b, rc, snap, bounds, state));
}

XentDl *flux_switch_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_switch(b, rc, snap, bounds, state));
}

XentDl *flux_radio_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_radio(b, rc, snap, bounds, state));
}

XentDl *flux_slider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_slider(b, rc, snap, bounds, state));
}

XentDl *flux_progress_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_progress(b, rc, snap, bounds));
}

XentDl *flux_scroll_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	return flux_container_record(rc, snap, bounds, state);
}

XentDl *flux_teachtip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxTeachingTipPaintInfo const *paint = snap->u.teach_tip.paint;
	return paint ? flux_teachtip_record_surface(rc, paint) : flux_container_record(rc, snap, bounds, state);
}

XentDl *flux_flip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxColor              bg = snap->background.rgba ? snap->background : t->flip_view_bg;
	FLUX_REC_END(flux_rec_fill_rect(b, bounds, bg));
}

XentDl *flux_tabs_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_tabs(b, rc, bounds));
}

XentDl *flux_tabitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_tabitem(b, rc, snap, bounds, state));
}

XentDl *flux_listbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t      = flux_rec_theme(rc);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxColor              bg     = snap->background.rgba ? snap->background : t->list_box_bg;
	FLUX_REC_END(flux_rec_fill_rounded(b, bounds, radius, bg));
}

XentDl *flux_dropdown_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_dropdown(b, rc, snap, bounds, state));
}

XentDl *flux_combo_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_combo(b, rc, snap, bounds, state));
}

XentDl *flux_split_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_split(b, rc, snap, bounds, state));
}

XentDl *flux_infobar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_infobar(b, rc, snap, bounds, state));
}

XentDl *flux_menuitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_menuitem(b, rc, snap, bounds, state));
}

XentDl *flux_crumb_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_breadcrumb(b, rc, snap, bounds, state));
}

XentDl *flux_selectoritem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_selectoritem(b, rc, snap, bounds, state));
}

XentDl *flux_pager_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_pager(b, rc, snap, bounds, state));
}

XentDl *flux_pips_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_pips(b, rc, snap, bounds, state));
}

XentDl *flux_exphead_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_exphead(b, rc, snap, bounds, state));
}

XentDl *flux_expbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_expbody(b, rc, snap, bounds));
}

XentDl *flux_dialogbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_dialogbody(b, rc, bounds));
}

XentDl *flux_flyout_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FluxThemeColors const *t      = flux_rec_theme(rc);
	float                  stroke = t->contrast.active ? 2.0f : FLUX_STROKE_WIDTH;
	FluxRect inset = {bounds->x + stroke / 2.0f, bounds->y + stroke / 2.0f, bounds->w - stroke, bounds->h - stroke};
	FLUX_REC_BEGIN();
	FLUX_REC_END(
	  flux_rec_fill_rounded(b, bounds, FLUX_FLYOUT_CORNER, t->flyout_background)
	  && flux_rec_stroke_rounded(b, &inset, FLUX_FLYOUT_CORNER, t->flyout_border, stroke)
	);
}

XentDl *flux_listitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_listitem(b, rc, snap, bounds, state));
}

XentDl *flux_listitem_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (!state->focused || !state->enabled || !flux_rec_theme(rc)->contrast.active) return NULL;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_listitem_focus(b, rc, snap, bounds));
}

XentDl *flux_list_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FluxListSnapshot const *ls = &snap->u.list;
	if (!ls->band) FLUX_REC_END(true);
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxRect               r  = {bounds->x + ls->rect.x, bounds->y + ls->rect.y, ls->rect.w, ls->rect.h};
	bool                   ok = xent_dl_pushopacity(b, 0.5f);
	if (ok) ok = flux_rec_fill_rect(b, &r, t->system_accent);
	if (ok) ok = flux_rec_stroke_rounded(b, &r, 0.0f, t->system_accent_light1, 1.0f);
	ok = xent_dl_popopacity(b) && ok;
	FLUX_REC_END(ok);
}

XentDl *flux_groupheader_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	FLUX_REC_BEGIN();
	FluxRect line  = {bounds->x + 12.0f, bounds->y + bounds->h - 5.0f, bounds->w - 24.0f, 1.0f};
	FluxRect frame = {bounds->x, bounds->y, bounds->w, bounds->h - 4.0f};
	bool     ok    = line.w <= 0.0f || flux_rec_fill_rect(b, &line, flux_rec_theme(rc)->base_low);
	if (ok && state->focused) ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &frame, 0.0f);
	FLUX_REC_END(ok);
}

XentDl *flux_treeitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_treeitem(b, rc, snap, bounds, state));
}

static XentImageFit image_fit(FluxImageStretch stretch) {
	if (stretch == FLUX_IMAGE_UNIFORM) return XENT_IMAGE_FIT_UNIFORM;
	if (stretch == FLUX_IMAGE_UNIFORM_TO_FILL) return XENT_IMAGE_FIT_UNIFORM_TO_FILL;
	if (stretch == FLUX_IMAGE_NONE) return XENT_IMAGE_FIT_NONE;
	return XENT_IMAGE_FIT_FILL;
}

static bool image_content_record(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds) {
	char const *path = snap->u.image.text_content;
	if (!path || !path [0]) return true;
	XentResourceId id = flux_resource_intern_path(path);
	bool           ok = xent_dl_pushcliprect(b, flux_rec_xrect(bounds));
	if (ok)
		ok = xent_dl_drawframe(
		  b, flux_rec_xrect(bounds), id, image_fit(snap->u.image.stretch), snap->u.image.frame, snap->u.image.nine
		);
	return xent_dl_popclip(b) && ok;
}

XentDl *flux_image_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) rc;
	( void ) state;
	FLUX_REC_BEGIN();
	bool ok = flux_rec_fill_bg(b, snap, bounds);
	if (ok) ok = image_content_record(b, snap, bounds);
	FLUX_REC_END(ok);
}

XentDl *flux_textbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_textbox_frame(b, rc, snap, bounds, state));
}

XentDl *flux_password_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_password(b, rc, snap, bounds, state));
}

XentDl *flux_number_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_number(b, rc, snap, bounds, state));
}

XentDl *flux_nav_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_nav(b, rc, snap, bounds));
}

XentDl *flux_navitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_navitem(b, rc, snap, bounds, state));
}

XentDl *flux_splitpane_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_splitpane(b, rc, snap, bounds));
}

XentDl *flux_titlebar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_titlebar(b, rc, snap, bounds, state));
}

XentDl *flux_rating_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_rating(b, rc, snap, bounds, state));
}

XentDl *flux_badge_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_badge(b, rc, snap, bounds));
}

XentDl *flux_progressring_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_progressring(b, rc, snap, bounds));
}

XentDl *flux_avatar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_avatar(b, rc, snap, bounds));
}

XentDl *flux_refresh_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) rc;
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_build_refresh(b, snap, bounds));
}

XentDl *flux_refresh_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FLUX_REC_BEGIN();
	FLUX_REC_END(flux_rec_refresh_overlay(b, rc, snap, bounds));
}
