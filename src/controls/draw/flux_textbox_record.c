#include "controls/draw/flux_control_record_internal.h"
#include "controls/textbox/tb_internal.h"
#include "controls/textbox/tb_metrics.h"
#include "fluxent/flux_text.h"
#include "render/flux_render_cache.h"
#include "render/flux_resource_intern.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TB_DELETE_BTN_W 30.0f

typedef struct TextboxComposedContent {
	char const *content;
	char        stack [512];
	char       *heap;
	int         composition_start_u16;
	bool        has_composition;
	bool        has_text;
} TextboxComposedContent;

static uint32_t utf8_bytes_to_utf16_count(char const *s, uint32_t byte_count) {
	if (!s || byte_count == 0) return 0;
	return ( uint32_t ) MultiByteToWideChar(CP_UTF8, 0, s, ( int ) byte_count, NULL, 0);
}

static char *tb_composed_buffer(TextboxComposedContent *out, int u8len) {
	if (u8len + 1 <= ( int ) sizeof(out->stack)) return out->stack;
	out->heap = ( char * ) malloc(( size_t ) u8len + 1);
	return out->heap;
}

static bool tb_set_utf8_from_wide(TextboxComposedContent *out, wchar_t const *text, int len) {
	int u8len = WideCharToMultiByte(CP_UTF8, 0, text, len, NULL, 0, NULL, NULL);
	if (u8len <= 0) return false;
	char *dst = tb_composed_buffer(out, u8len);
	if (!dst) return false;
	WideCharToMultiByte(CP_UTF8, 0, text, len, dst, u8len, NULL, NULL);
	dst [u8len]  = 0;
	out->content = dst;
	return true;
}

static bool tb_compose_with_existing(TextboxComposedContent *out, FluxRenderSnap const *snap) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, snap->u.textbox.text_content, -1, NULL, 0);
	if (wlen <= 0) return false;

	wchar_t *wtmp = ( wchar_t * ) malloc(( size_t ) wlen * sizeof(wchar_t));
	if (!wtmp) return false;
	MultiByteToWideChar(CP_UTF8, 0, snap->u.textbox.text_content, -1, wtmp, wlen);

	int orig_wlen = wlen - 1;
	int start = ( int ) utf8_bytes_to_utf16_count(snap->u.textbox.text_content, snap->u.textbox.edit.cursor_position);
	if (start > orig_wlen) start = orig_wlen;

	int      result_wlen = orig_wlen + ( int ) snap->u.textbox.edit.composition_length;
	wchar_t *wresult     = ( wchar_t * ) malloc((( size_t ) result_wlen + 1) * sizeof(wchar_t));
	if (!wresult) {
		free(wtmp);
		return false;
	}

	memcpy(wresult, wtmp, ( size_t ) start * sizeof(wchar_t));
	memcpy(
	  wresult + start, snap->u.textbox.edit.composition_text,
	  ( size_t ) snap->u.textbox.edit.composition_length * sizeof(wchar_t)
	);
	memcpy(
	  wresult + start + snap->u.textbox.edit.composition_length, wtmp + start,
	  ( size_t ) (orig_wlen - start) * sizeof(wchar_t)
	);
	wresult [result_wlen]      = 0;

	bool ok                    = tb_set_utf8_from_wide(out, wresult, result_wlen);
	out->composition_start_u16 = ok ? start : 0;
	free(wresult);
	free(wtmp);
	return ok;
}

static bool tb_compose_from_composition(TextboxComposedContent *out, FluxRenderSnap const *snap) {
	out->composition_start_u16 = 0;
	return tb_set_utf8_from_wide(
	  out, snap->u.textbox.edit.composition_text, ( int ) snap->u.textbox.edit.composition_length
	);
}

static void tb_prepare_content(TextboxComposedContent *out, FluxRenderSnap const *snap) {
	memset(out, 0, sizeof(*out));
	out->content         = snap->u.textbox.text_content;
	out->has_composition = snap->u.textbox.edit.composition_text && snap->u.textbox.edit.composition_length > 0;
	if (!out->has_composition) {
		out->has_text = out->content && out->content [0];
		return;
	}

	bool ok = (snap->u.textbox.text_content && snap->u.textbox.text_content [0])
	          ? tb_compose_with_existing(out, snap)
	          : tb_compose_from_composition(out, snap);
	if (!ok) out->content = snap->u.textbox.text_content;
	out->has_text = out->content && out->content [0];
}

static void          tb_free_content(TextboxComposedContent *content) { free(content->heap); }

static FluxTextStyle tb_hit_style(FluxRenderSnap const *snap) {
	FluxTextStyle hit;
	memset(&hit, 0, sizeof(hit));
	hit.font_family = snap->u.textbox.font_family;
	hit.font_size   = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	hit.vert_align  = FLUX_TEXT_TOP;
	hit.word_wrap   = snap->u.textbox.edit.multiline;
	return hit;
}

static bool
tb_elevation_border(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, float radius, bool focused) {
	FluxThemeColors const *t  = flux_rec_theme(rc);
	float                  y0 = bounds->y + bounds->h;
	float                  y1 = bounds->y + bounds->h - 2.0f;
	if (focused) {
		XentLinearGrad grad = {
		  0.0f, y0, 0.0f, y1, 1.0f, 1.0f, flux_rec_xcolor(t->accent_default), flux_rec_xcolor(t->ctrl_stroke_default)};
		return xent_dl_strokerrectgrad(b, flux_rec_xrect(bounds), radius, 1.0f, &grad);
	}
	XentLinearGrad grad = {
	  0.0f, y0, 0.0f, y1, 0.5f, 1.0f, flux_rec_xcolor(t->ctrl_strong_stroke_default),
	  flux_rec_xcolor(t->ctrl_stroke_default)};
	return xent_dl_strokerrectgrad(b, flux_rec_xrect(bounds), radius, 1.0f, &grad);
}

static bool tb_focus_accent(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, float radius) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (!xent_dl_pushcliprrect(b, flux_rec_xrect(bounds), radius)) return false;
	float line_y = bounds->y + bounds->h - 1.0f;
	bool  ok     = xent_dl_strokeline(
	  b, bounds->x, line_y, bounds->x + bounds->w, line_y, flux_rec_xcolor(t->accent_default), 2.0f,
	  XENT_STROKE_CAP_FLAT
	);
	return xent_dl_popclip(b) && ok;
}

static bool tb_draw_frame(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, FluxControlState const *state, float radius
) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (!state->enabled) return flux_rec_stroke_rounded(b, bounds, radius, t->ctrl_stroke_default, 1.0f);
	bool ok = tb_elevation_border(b, rc, bounds, radius, state->focused);
	if (ok && state->focused) ok = tb_focus_accent(b, rc, bounds, radius);
	return ok;
}

static bool tb_draw_selection(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *text_area,
  FluxControlState const *state, TextboxComposedContent const *content
) {
	if (!content->has_text || !state->focused) return true;
	if (snap->u.textbox.edit.selection_start == snap->u.textbox.edit.selection_end) return true;
	if (!rc || !rc->text) return true;

	uint32_t a = snap->u.textbox.edit.selection_start;
	uint32_t e = snap->u.textbox.edit.selection_end;
	if (a > e) {
		uint32_t tmp = a;
		a            = e;
		e            = tmp;
	}
	char const *committed = snap->u.textbox.text_content;
	int         sel_s     = ( int ) utf8_bytes_to_utf16_count(committed, a);
	int         sel_e     = ( int ) utf8_bytes_to_utf16_count(committed, e);
	if (content->has_composition && sel_s >= content->composition_start_u16) {
		sel_s += ( int ) snap->u.textbox.edit.composition_length;
		sel_e += ( int ) snap->u.textbox.edit.composition_length;
	}
	else if (content->has_composition && sel_e > content->composition_start_u16) {
		sel_e += ( int ) snap->u.textbox.edit.composition_length;
	}

	FluxTextStyle          hit = tb_hit_style(snap);
	FluxRect               rects [16];
	FluxTextSelectionQuery q = {
	  {rc->text, content->content, &hit, text_area->w},
      sel_s, sel_e, rects, 16
    };
	uint32_t               n = flux_text_selection(&q);
	FluxThemeColors const *t = flux_rec_theme(rc);
	FluxColor sel = flux_color_af(snap->u.textbox.edit.selection_color) > 0.0f ? snap->u.textbox.edit.selection_color
	                                                                           : t->accent_default;
	sel.rgba      = (sel.rgba & 0xffffff00u) | 0x66u;
	for (uint32_t i = 0; i < n; i++) {
		FluxRect sr = {
		  text_area->x + rects [i].x - snap->u.textbox.edit.scroll_offset_x,
		  text_area->y + rects [i].y - snap->u.textbox.edit.scroll_offset_y, rects [i].w, rects [i].h};
		if (!flux_rec_fill_rect(b, &sr, sel)) return false;
	}
	return true;
}

static bool tb_draw_composition_underline(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *text_area,
  FluxControlState const *state, TextboxComposedContent const *content
) {
	if (!content->has_composition || !content->has_text || !rc || !rc->text) return true;

	int                    comp_end = content->composition_start_u16 + ( int ) snap->u.textbox.edit.composition_length;
	FluxTextStyle          hit      = tb_hit_style(snap);
	FluxRect               rects [16];
	FluxTextSelectionQuery q = {
	  {rc->text, content->content, &hit, text_area->w},
      content->composition_start_u16, comp_end, rects, 16
    };
	uint32_t               n  = flux_text_selection(&q);
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxColor              ul = state->enabled ? t->text_primary : t->text_disabled;
	for (uint32_t i = 0; i < n; i++) {
		float lx0 = text_area->x + rects [i].x - snap->u.textbox.edit.scroll_offset_x;
		float ly  = text_area->y + rects [i].y + rects [i].h - snap->u.textbox.edit.scroll_offset_y;
		float lx1 = lx0 + rects [i].w;
		if (!flux_rec_line_h(b, lx0, lx1, ly - 0.5f, 1.0f, ul)) return false;
	}
	return true;
}

static XentTextStyle tb_content_style(FluxRenderSnap const *snap) {
	float size      = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	bool  multiline = snap->u.textbox.edit.multiline;
	return (XentTextStyle) {
	  .font_size   = size,
	  .font_weight = 400u,
	  .h_align     = 0u,
	  .v_align     = multiline ? 0u : 1u,
	  .flags       = multiline ? XENT_DL_TEXT_WRAP : 0u,
	  .font_family = snap->u.textbox.font_family,
	};
}

static bool tb_caret_visible(FluxRenderCtx const *rc, FluxRenderSnap const *snap, int caret_u16) {
	FluxCacheEntry *ce = rc->cache ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (!ce) return true;

	float cur = ( float ) caret_u16;
	if (!ce->caret_blink_initialized || ce->caret_blink_index != cur) {
		ce->caret_blink_epoch       = rc->now;
		ce->caret_blink_index       = cur;
		ce->caret_blink_initialized = true;
	}
	double elapsed = rc->now - ce->caret_blink_epoch;
	int    phase   = ( int ) (elapsed / 0.5);
	if (rc->animations_active) *rc->animations_active = true;
	return (phase % 2) == 0;
}

static void tb_reset_caret_blink(FluxRenderCtx const *rc, FluxRenderSnap const *snap) {
	if (!rc || !rc->cache) return;
	FluxCacheEntry *ce = flux_render_cache_get(rc->cache, snap->id);
	if (ce) ce->caret_blink_initialized = false;
}

static int tb_caret_index(FluxRenderSnap const *snap, TextboxComposedContent const *content) {
	if (content->has_composition)
		return content->composition_start_u16 + ( int ) snap->u.textbox.edit.composition_cursor;
	return ( int ) utf8_bytes_to_utf16_count(snap->u.textbox.text_content, snap->u.textbox.edit.cursor_position);
}

static float tb_clamp_caret_x(FluxRect const *text_area, float x) {
	float min_x = text_area->x;
	float max_x = text_area->x + text_area->w - 1.0f;
	if (max_x < min_x) max_x = min_x;
	if (x < min_x) return min_x;
	if (x > max_x) return max_x;
	return x;
}

static bool tb_draw_caret(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *text_area,
  FluxControlState const *state, TextboxComposedContent const *content
) {
	if (!state->focused || snap->u.textbox.edit.readonly || !rc || !rc->text) return true;
	if (!content->has_composition && snap->u.textbox.edit.selection_start != snap->u.textbox.edit.selection_end)
		return true;

	int caret_u16 = tb_caret_index(snap, content);
	if (!tb_caret_visible(rc, snap, caret_u16)) return true;

	float font_size     = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	float caret_h       = font_size * 1.2f;
	float caret_x_local = 0.0f;
	float caret_y_local = 0.0f;
	if (content->has_text) {
		FluxTextStyle      hit   = tb_hit_style(snap);
		FluxTextCaretQuery query = {
		  {rc->text, content->content, &hit, text_area->w},
          caret_u16
        };
		FluxRect caret = flux_text_caret(&query);
		if (caret.h > 0.0f) caret_h = caret.h;
		caret_x_local = caret.x;
		caret_y_local = caret.y;
	}

	float caret_y = content->has_text ? text_area->y + caret_y_local - snap->u.textbox.edit.scroll_offset_y
	                                  : text_area->y + (text_area->h - caret_h) * 0.5f;
	float caret_x = tb_clamp_caret_x(text_area, text_area->x + caret_x_local - snap->u.textbox.edit.scroll_offset_x);
	FluxThemeColors const *t       = flux_rec_theme(rc);
	FluxRect               caret_r = {caret_x, caret_y, 1.0f, caret_h};
	if (rc->animations_active) *rc->animations_active = true;
	return flux_rec_fill_rect(b, &caret_r, t->text_primary);
}

static bool tb_draw_content(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *text_area,
  FluxControlState const *state
) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(text_area))) return false;

	TextboxComposedContent content;
	tb_prepare_content(&content, snap);

	bool          ok    = true;
	XentTextStyle style = tb_content_style(snap);
	if (!content.has_text && snap->u.textbox.placeholder && snap->u.textbox.placeholder [0]) {
		FluxColor ph = state->enabled ? t->text_secondary : t->text_disabled;
		ok           = flux_rec_text_ex(b, text_area, snap->u.textbox.placeholder, &style, ph);
	}
	if (ok) ok = tb_draw_selection(b, rc, snap, text_area, state, &content);
	if (ok) ok = tb_draw_composition_underline(b, rc, snap, text_area, state, &content);
	if (ok && content.has_text) {
		FluxRect draw = {
		  text_area->x - snap->u.textbox.edit.scroll_offset_x, text_area->y - snap->u.textbox.edit.scroll_offset_y,
		  text_area->w + snap->u.textbox.edit.scroll_offset_x, text_area->h + snap->u.textbox.edit.scroll_offset_y};
		FluxColor fg = state->enabled ? t->text_primary : t->text_disabled;
		if (flux_color_af(snap->u.textbox.text_color) > 0.0f) fg = snap->u.textbox.text_color;
		ok = flux_rec_text_ex(b, &draw, content.content, &style, fg);
	}
	if (ok) ok = tb_draw_caret(b, rc, snap, text_area, state, &content);
	if (!state->focused) tb_reset_caret_blink(rc, snap);

	tb_free_content(&content);
	return xent_dl_popclip(b) && ok;
}

static bool tb_draw_delete(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state, float radius, float col0_w, float col1_w
) {
	FluxRect btn = {bounds->x + col0_w, bounds->y + 4.0f, col1_w - 4.0f, bounds->h - 8.0f};
	if (btn.w < 0.0f) btn.w = 0.0f;
	if (btn.h < 0.0f) btn.h = 0.0f;
	bool                   hovered = state->hovered && snap->hover_local_x >= col0_w;
	bool                   pressed = hovered && state->pressed;
	FluxThemeColors const *t       = flux_rec_theme(rc);
	bool                   ok      = true;
	if (pressed) ok = flux_rec_fill_rounded(b, &btn, radius, t->ctrl_alt_fill_tertiary);
	else if (hovered) ok = flux_rec_fill_rounded(b, &btn, radius, t->ctrl_alt_fill_secondary);
	char const *x  = "\xEE\xA2\x94";
	FluxColor   ic = pressed ? t->text_tertiary : t->text_secondary;
	if (ok)
		ok = flux_rec_text_ex(
		  b, &btn, x,
		  &(XentTextStyle) {
			.font_size   = 12.0f,
			.font_weight = 400u,
			.h_align     = 1u,
			.v_align     = 1u,
			.flags       = XENT_DL_TEXT_ICON,
			.font_family = flux_icon_font_family(),
		  },
		  ic
		);
	return ok;
}

static bool tb_draw_combo_overlay(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool in_zone = state->hovered && snap->hover_local_x >= bounds->w - FLUX_COMBO_GLYPH_COL;
	if (in_zone && state->enabled) {
		FluxRect  ov   = {bounds->x + bounds->w - 34.0f, bounds->y + 4.0f, 30.0f, bounds->h - 8.0f};
		FluxColor pr   = state->focused ? t->ctrl_alt_fill_quarternary : t->subtle_fill_tertiary;
		FluxColor fill = state->pressed ? pr : t->subtle_fill_secondary;
		if (!flux_rec_fill_rounded(b, &ov, 4.0f, fill)) return false;
	}
	FluxRect glyph = {bounds->x + bounds->w - 26.0f, bounds->y, 12.0f, bounds->h};
	return flux_rec_glyph_wc(b, &glyph, L"\xE70D", 12.0f, state->enabled ? t->text_secondary : t->text_disabled);
}

static bool tb_combo_bare(FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state) {
	bool text_hover = state->hovered && snap->hover_local_x < bounds->w - FLUX_COMBO_GLYPH_COL;
	return snap->u.textbox.combo_editor && state->enabled && !state->focused && !text_hover;
}

static bool tb_show_delete(FluxRenderSnap const *snap, FluxControlState const *state, bool has_text) {
	if (snap->type != FLUX_CONTROL_TEXT_INPUT || snap->u.textbox.combo_editor) return false;
	return has_text && state->focused && state->enabled && !snap->u.textbox.edit.readonly;
}

bool flux_rec_textbox_frame(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (bounds->w <= 0.0f || bounds->h <= 0.0f) return true;
	float                  radius   = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   has_text = snap->u.textbox.text_content && snap->u.textbox.text_content [0];
	bool                   combo    = snap->u.textbox.combo_editor;
	bool                   show_del = tb_show_delete(snap, state, has_text);
	float                  col1_w   = combo ? FLUX_COMBO_EDITOR_RESERVE : show_del ? TB_DELETE_BTN_W : 0.0f;
	float                  col0_w   = flux_maxf(0.0f, bounds->w - col1_w);

	FluxThemeColors const *t        = flux_rec_theme(rc);
	FluxColor              fill     = flux_input_fill_color(t, state->enabled, state->focused, state->hovered);
	( void ) flux_rec_animstate(rc, snap, state, false);
	fill      = flux_rec_tweenfill(rc, snap, fill);
	bool bare = tb_combo_bare(snap, bounds, state);
	bool ok   = bare || flux_rec_fillsink(b, rc, bounds, radius, fill);
	if (ok && !bare) ok = tb_draw_frame(b, rc, bounds, state, radius);

	FluxRect tb = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, col0_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};
	if (ok) ok = tb_draw_content(b, rc, snap, &tb, state);
	if (ok && show_del) ok = tb_draw_delete(b, rc, snap, bounds, state, radius, col0_w, col1_w);
	if (ok && combo) ok = tb_draw_combo_overlay(b, t, snap, bounds, state);
	return ok;
}

static FluxRenderSnap
pb_masked_snapshot(FluxRenderSnap const *snap, bool has_text, bool revealed, char *mask_buf, size_t mask_cap) {
	FluxRenderSnap masked = *snap;
	if (!has_text || revealed) return masked;

	uint32_t text_len = ( uint32_t ) strlen(snap->u.textbox.text_content);
	pb_build_mask(snap->u.textbox.text_content, text_len, mask_buf, ( uint32_t ) mask_cap);
	masked.u.textbox.text_content = mask_buf;
	masked.u.textbox.edit.cursor_position
	  = pb_original_offset_to_mask(snap->u.textbox.text_content, text_len, snap->u.textbox.edit.cursor_position);
	masked.u.textbox.edit.selection_start
	  = pb_original_offset_to_mask(snap->u.textbox.text_content, text_len, snap->u.textbox.edit.selection_start);
	masked.u.textbox.edit.selection_end
	  = pb_original_offset_to_mask(snap->u.textbox.text_content, text_len, snap->u.textbox.edit.selection_end);
	masked.u.textbox.edit.composition_text   = NULL;
	masked.u.textbox.edit.composition_length = 0;
	masked.u.textbox.edit.composition_cursor = 0;
	return masked;
}

static bool password_record_reveal(
  XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state,
  FluxThemeColors const *t, float radius, float text_width
) {
	FluxRect btn     = {bounds->x + text_width, bounds->y + 4.0f, FLUX_PASSWORD_REVEAL_BTN_W - 4.0f, bounds->h - 8.0f};
	bool     hovered = state->hovered && snap->hover_local_x >= text_width;
	bool     pressed = hovered && state->pressed;
	bool     ok      = true;
	if (pressed) ok = flux_rec_fill_rounded(b, &btn, radius, t->ctrl_alt_fill_tertiary);
	else if (hovered) ok = flux_rec_fill_rounded(b, &btn, radius, t->ctrl_alt_fill_secondary);
	if (!ok) return false;

	XentTextStyle style = {
	  .font_size   = 12.0f,
	  .font_weight = 400u,
	  .h_align     = 1u,
	  .v_align     = 1u,
	  .flags       = XENT_DL_TEXT_ICON,
	  .font_family = flux_icon_font_family(),
	};
	FluxColor color = pressed ? t->text_tertiary : t->text_secondary;
	return flux_rec_text_ex(b, &btn, "\xEF\x9E\x8D", &style, color);
}

bool flux_rec_password(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	float                  radius      = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   has_text    = snap->u.textbox.text_content && snap->u.textbox.text_content [0];
	bool                   show_reveal = state->enabled && has_text && state->focused;
	bool                   revealed    = snap->u.textbox.is_checked;
	float                  col1_w      = show_reveal ? FLUX_PASSWORD_REVEAL_BTN_W : 0.0f;
	float                  col0_w      = flux_maxf(0.0f, bounds->w - col1_w);

	FluxThemeColors const *t           = flux_rec_theme(rc);
	FluxColor              fill        = flux_input_fill_color(t, state->enabled, state->focused, state->hovered);
	( void ) flux_rec_animstate(rc, snap, state, false);
	fill    = flux_rec_tweenfill(rc, snap, fill);
	bool ok = flux_rec_fillsink(b, rc, bounds, radius, fill);
	if (ok) ok = tb_draw_frame(b, rc, bounds, state, radius);

	char           mask_buf [2048];
	FluxRenderSnap masked = pb_masked_snapshot(snap, has_text, revealed, mask_buf, sizeof(mask_buf));

	FluxRect       tb     = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, col0_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};
	if (ok) ok = tb_draw_content(b, rc, &masked, &tb, state);

	if (ok && show_reveal) ok = password_record_reveal(b, snap, bounds, state, t, radius, col0_w);
	return ok;
}

#define NB_ZONE_NONE     (-1)
#define NB_ZONE_TEXT     0
#define NB_ZONE_SPIN_UP  1
#define NB_ZONE_SPIN_DN  2
#define NB_ZONE_SPIN_GAP 3
#define NB_ZONE_DELETE   4

typedef struct NbMargins {
	float l, t, r, b;
} NbMargins;

static NbMargins const NB_UP_M
  = {FLUX_NUMBER_BOX_UP_MARGIN, FLUX_NUMBER_BOX_UP_MARGIN, FLUX_NUMBER_BOX_UP_MARGIN, FLUX_NUMBER_BOX_UP_MARGIN};
static NbMargins const NB_DN_M = {
  FLUX_NUMBER_BOX_DN_MARGIN_L, FLUX_NUMBER_BOX_DN_MARGIN_T, FLUX_NUMBER_BOX_DN_MARGIN_R, FLUX_NUMBER_BOX_DN_MARGIN_B};
static NbMargins const NB_INNER_M = {0.0f, 4.0f, 4.0f, 4.0f};

static FluxRect        nb_inner(FluxRect const *outer, NbMargins m) {
	return (FluxRect) {outer->x + m.l, outer->y + m.t, outer->w - m.l - m.r, outer->h - m.t - m.b};
}

static int nb_hover_zone(float lx, float bounds_w, bool show_delete, bool spin_visible) {
	if (lx < 0.0f) return NB_ZONE_NONE;
	float spin_start = spin_visible ? bounds_w - FLUX_NUMBER_BOX_SPIN_W : bounds_w;
	float del_start  = show_delete ? spin_start - FLUX_NUMBER_BOX_DELETE_BTN_W : spin_start;
	if (lx < del_start) return NB_ZONE_TEXT;
	if (show_delete && lx < spin_start) return NB_ZONE_DELETE;
	if (!spin_visible || lx < spin_start) return NB_ZONE_TEXT;
	float up_x0 = spin_start + FLUX_NUMBER_BOX_UP_MARGIN;
	float up_x1 = up_x0 + FLUX_NUMBER_BOX_SPIN_BTN_W;
	if (lx >= up_x0 && lx < up_x1) return NB_ZONE_SPIN_UP;
	float dn_x0 = spin_start + FLUX_NUMBER_BOX_COL_UP_W + FLUX_NUMBER_BOX_DN_MARGIN_L;
	float dn_x1 = dn_x0 + FLUX_NUMBER_BOX_SPIN_BTN_W;
	if (lx >= dn_x0 && lx < dn_x1) return NB_ZONE_SPIN_DN;
	return NB_ZONE_SPIN_GAP;
}

static FluxColor nb_action_icon_color(bool enabled, bool pressed, FluxThemeColors const *t) {
	if (!enabled) return t->text_disabled;
	if (pressed) return t->text_tertiary;
	return t->text_secondary;
}

static bool nb_draw_inner_button(
  XentDlBuilder *b, FluxRect const *outer, NbMargins margin, float radius, bool hovered, bool pressed,
  FluxThemeColors const *t
) {
	FluxRect r = nb_inner(outer, margin);
	if (r.w <= 0.0f || r.h <= 0.0f) return true;
	if (pressed) return flux_rec_fill_rounded(b, &r, radius, t->ctrl_alt_fill_tertiary);
	if (hovered) return flux_rec_fill_rounded(b, &r, radius, t->ctrl_alt_fill_secondary);
	return true;
}

static bool nb_draw_icon(
  XentDlBuilder *b, FluxRect const *outer, NbMargins margin, char const *icon_utf8, float font_size, FluxColor color
) {
	FluxRect r = nb_inner(outer, margin);
	if (r.w <= 0.0f || r.h <= 0.0f) return true;
	return flux_rec_text_ex(
	  b, &r, icon_utf8,
	  &(XentTextStyle) {
		.font_size   = font_size,
		.font_weight = 400u,
		.h_align     = 1u,
		.v_align     = 1u,
		.flags       = XENT_DL_TEXT_ICON,
		.font_family = flux_icon_font_family(),
	  },
	  color
	);
}

static bool nb_draw_delete(
  XentDlBuilder *b, FluxRect const *bounds, float text_col_w, float radius, int zone, bool pressed,
  FluxThemeColors const *t
) {
	FluxRect del_outer = {bounds->x + text_col_w, bounds->y, FLUX_NUMBER_BOX_DELETE_BTN_W, bounds->h};
	bool     hot       = zone == NB_ZONE_DELETE;
	bool     press     = hot && pressed;
	bool     ok        = nb_draw_inner_button(b, &del_outer, NB_INNER_M, radius, hot, press, t);
	return ok && nb_draw_icon(b, &del_outer, NB_INNER_M, "\xEE\xA2\x94", 12.0f, nb_action_icon_color(true, press, t));
}

static bool nb_draw_spin(
  XentDlBuilder *b, FluxRect const *bounds, float radius, int zone, bool pressed, FluxRenderSnap const *snap,
  FluxThemeColors const *t
) {
	float    spin_base_x = bounds->x + bounds->w - FLUX_NUMBER_BOX_SPIN_W;
	FluxRect up_outer    = {spin_base_x, bounds->y, FLUX_NUMBER_BOX_COL_UP_W, bounds->h};
	FluxRect dn_outer    = {spin_base_x + FLUX_NUMBER_BOX_COL_UP_W, bounds->y, FLUX_NUMBER_BOX_COL_DN_W, bounds->h};

	bool     up_hot      = zone == NB_ZONE_SPIN_UP && snap->u.textbox.nb_up_enabled;
	bool     dn_hot      = zone == NB_ZONE_SPIN_DN && snap->u.textbox.nb_down_enabled;
	bool     up_press    = up_hot && pressed;
	bool     dn_press    = dn_hot && pressed;

	bool     ok          = nb_draw_inner_button(b, &up_outer, NB_UP_M, radius, up_hot, up_press, t);
	if (ok) ok = nb_draw_inner_button(b, &dn_outer, NB_DN_M, radius, dn_hot, dn_press, t);

	float icon_fs = snap->font_size > 0.0f ? snap->font_size : 14.0f;
	if (ok)
		ok = nb_draw_icon(
		  b, &up_outer, NB_UP_M, "\xEE\x9C\x8E", icon_fs,
		  nb_action_icon_color(snap->u.textbox.nb_up_enabled, up_press, t)
		);
	if (ok)
		ok = nb_draw_icon(
		  b, &dn_outer, NB_DN_M, "\xEE\x9C\x8D", icon_fs,
		  nb_action_icon_color(snap->u.textbox.nb_down_enabled, dn_press, t)
		);
	return ok;
}

bool flux_rec_number(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool  spin_visible = snap->u.textbox.nb_spin_placement == FLUX_NB_SPIN_INLINE;
	bool  has_text     = snap->u.textbox.text_content && snap->u.textbox.text_content [0];
	bool  show_del     = has_text && state->focused && !snap->u.textbox.edit.readonly && state->enabled;
	float spin_w       = spin_visible ? FLUX_NUMBER_BOX_SPIN_W : 0.0f;
	float del_w        = show_del ? FLUX_NUMBER_BOX_DELETE_BTN_W : 0.0f;
	float text_col_w   = flux_maxf(0.0f, bounds->w - del_w - spin_w);
	float radius       = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;

	int   zone         = NB_ZONE_NONE;
	if (state->hovered) zone = nb_hover_zone(snap->hover_local_x, bounds->w, show_del, spin_visible);

	FluxThemeColors const *t          = flux_rec_theme(rc);
	bool                   chrome_hov = zone == NB_ZONE_TEXT;
	FluxColor              fill       = flux_input_fill_color(t, state->enabled, state->focused, chrome_hov);
	( void ) flux_rec_animstate(rc, snap, state, false);
	fill    = flux_rec_tweenfill(rc, snap, fill);
	bool ok = flux_rec_fillsink(b, rc, bounds, radius, fill);
	if (ok) ok = tb_draw_frame(b, rc, bounds, state, radius);

	FluxRect tb = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, text_col_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};
	FluxControlState text_state = *state;
	if (zone != NB_ZONE_TEXT && !state->focused) text_state.hovered = 0;
	if (ok) ok = tb_draw_content(b, rc, snap, &tb, &text_state);

	if (!state->enabled) return ok;
	if (ok && show_del) ok = nb_draw_delete(b, bounds, text_col_w, radius, zone, state->pressed, t);
	if (ok && spin_visible) ok = nb_draw_spin(b, bounds, radius, zone, state->pressed, snap, t);
	return ok;
}
