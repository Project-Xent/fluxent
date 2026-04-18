/**
 * @file tb_scroll.c
 * @brief Scroll and IME position management for TextBox.
 */
#include "tb_internal.h"
#include "fluxent/fluxent.h"
#include <string.h>

FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = tb->base.font_family;
	ts.font_size   = tb->base.font_size > 0.0f ? tb->base.font_size : 14.0f;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.word_wrap   = false;
	return ts;
}

void tb_update_scroll(FluxTextBoxInputData *tb) {
	FluxTextRenderer *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tr) return;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	float visible_w = rect.width - 10.0f - 6.0f; /* FLUX_TEXTBOX_PAD_L + PAD_R */
	if (visible_w <= 0.0f) return;

	FluxTextStyle ts         = tb_make_style(tb);
	uint32_t      u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->base.cursor_position);
	FluxRect      caret
	  = flux_text_caret_rect(tr, tb->buffer, &ts, visible_w + tb->base.scroll_offset_x, ( int ) u16_cursor);
	float cx = caret.x;

	if (cx - tb->base.scroll_offset_x < 0.0f) tb->base.scroll_offset_x = cx;
	else if (cx - tb->base.scroll_offset_x > visible_w) tb->base.scroll_offset_x = cx - visible_w;
	if (tb->base.scroll_offset_x < 0.0f) tb->base.scroll_offset_x = 0.0f;
}

void tb_notify_change(FluxTextBoxInputData *tb) {
	if (tb->base.on_change) tb->base.on_change(tb->base.on_change_ctx, tb->buffer);
}

void tb_update_ime_position(FluxTextBoxInputData *tb) {
	FluxWindow       *window = tb->app ? flux_app_get_window(tb->app) : NULL;
	FluxTextRenderer *tr     = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!window || !tr) return;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);

	/* Compute caret position in textbox-local coords (TOP-aligned for correct Y) */
	FluxTextStyle ts = tb_make_style(tb);
	ts.vert_align    = FLUX_TEXT_TOP;
	float visible_w  = rect.width - 10.0f - 6.0f;

	/*
	 * During IME composition the real visual caret sits AFTER the
	 * composition text, but cursor_position (byte offset) hasn't
	 * moved yet.  Build a temporary display string that includes
	 * the composition, then hit-test at (composition_start + comp_cursor).
	 */
	float caret_x = 0.0f, caret_y = 0.0f, caret_h = 0.0f;

		if (tb->base.composition_text && tb->base.composition_length > 0 && tb->buffer [0]) {
			/* Convert original text to wide */
			int      orig_wlen = MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, NULL, 0) - 1;
			int      comp_len  = ( int ) tb->base.composition_length;
			int      total     = orig_wlen + comp_len + 1;
			wchar_t *wtmp      = ( wchar_t * ) _alloca(total * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, wtmp, orig_wlen + 1);

			int ins = ( int ) MultiByteToWideChar(CP_UTF8, 0, tb->buffer, ( int ) tb->base.cursor_position, NULL, 0);
			if (ins > orig_wlen) ins = orig_wlen;

			/* [0..ins] + composition + [ins..end] */
			wchar_t *composed = ( wchar_t * ) _alloca((orig_wlen + comp_len + 1) * sizeof(wchar_t));
			memcpy(composed, wtmp, ins * sizeof(wchar_t));
			memcpy(composed + ins, tb->base.composition_text, comp_len * sizeof(wchar_t));
			memcpy(composed + ins + comp_len, wtmp + ins, (orig_wlen - ins) * sizeof(wchar_t));
			composed [orig_wlen + comp_len] = 0;

			/* Convert back to UTF-8 for caret_rect */
			int   u8len = WideCharToMultiByte(CP_UTF8, 0, composed, orig_wlen + comp_len, NULL, 0, NULL, NULL);
			char *u8tmp = ( char * ) _alloca(u8len + 1);
			WideCharToMultiByte(CP_UTF8, 0, composed, orig_wlen + comp_len, u8tmp, u8len, NULL, NULL);
			u8tmp [u8len]    = 0;

			int      hit_u16 = ins + ( int ) tb->base.composition_cursor;
			FluxRect caret   = flux_text_caret_rect(tr, u8tmp, &ts, visible_w + tb->base.scroll_offset_x, hit_u16);
			caret_x          = caret.x;
			caret_y          = caret.y;
			caret_h          = caret.h;
		}
		else {
			uint32_t u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->base.cursor_position);
			FluxRect caret
			  = flux_text_caret_rect(tr, tb->buffer, &ts, visible_w + tb->base.scroll_offset_x, ( int ) u16_cursor);
			caret_x = caret.x;
			caret_y = caret.y;
			caret_h = caret.h;
		}

	/* Textbox absolute position + padding + caret offset - scroll */
	float       cx    = rect.x + 10.0f + caret_x - tb->base.scroll_offset_x;
	float       cy    = rect.y + 5.0f + caret_y;
	float       ch    = caret_h > 0.0f ? caret_h : (ts.font_size > 0.0f ? ts.font_size : 14.0f) * 1.2f;

	/* Convert from DIPs to physical pixels */
	FluxDpiInfo dpi   = flux_window_dpi(window);
	float       scale = dpi.dpi_x / 96.0f;

	flux_window_set_ime_position(window, ( int ) (cx * scale), ( int ) (cy * scale), ( int ) (ch * scale));
}
