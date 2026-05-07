#include "tb_internal.h"
#include "fluxent/fluxent.h"
#include <stdlib.h>
#include <string.h>

#define TB_CM_CUT           1
#define TB_CM_COPY          2
#define TB_CM_PASTE         3
#define TB_CM_SELALL        4
#define TB_MASK_STACK_BYTES 2048

static bool tb_encode_utf8(wchar_t *high, wchar_t ch, char out [4], int *outlen) {
	if (ch >= 0xd800 && ch <= 0xdbff) {
		*high = ch;
		return false;
	}
	if (ch >= 0xdc00 && ch <= 0xdfff) {
		if (*high == 0) return false;
		uint32_t cp = 0x10000u + ((( uint32_t ) (*high - 0xd800u)) << 10) + ( uint32_t ) (ch - 0xdc00u);
		*high       = 0;
		out [0]     = ( char ) (0xf0 | (cp >> 18));
		out [1]     = ( char ) (0x80 | ((cp >> 12) & 0x3f));
		out [2]     = ( char ) (0x80 | ((cp >> 6) & 0x3f));
		out [3]     = ( char ) (0x80 | (cp & 0x3f));
		*outlen     = 4;
		return true;
	}
	*high = 0;
	if (ch < 0x80) {
		out [0] = ( char ) ch;
		*outlen = 1;
	}
	else if (ch < 0x800) {
		out [0] = ( char ) (0xc0 | (ch >> 6));
		out [1] = ( char ) (0x80 | (ch & 0x3f));
		*outlen = 2;
	}
	else {
		out [0] = ( char ) (0xe0 | (ch >> 12));
		out [1] = ( char ) (0x80 | ((ch >> 6) & 0x3f));
		out [2] = ( char ) (0x80 | (ch & 0x3f));
		*outlen = 3;
	}
	return true;
}

static bool tb_show_trailing_button(FluxTextBoxInputData const *tb, FluxNodeData const *nd, bool allow_readonly) {
	if (tb->buf_len == 0 || !nd || !nd->state.focused) return false;
	return allow_readonly || !tb->base.readonly;
}

static float tb_reserved_trailing_w(FluxTextBoxInputData const *tb, XentControlType ct, FluxNodeData const *nd) {
	bool show_clear  = tb_show_trailing_button(tb, nd, false);
	bool show_reveal = tb_show_trailing_button(tb, nd, true);

	switch (ct) {
	case XENT_CONTROL_NUMBER_BOX   : return (tb->nb->spin_placement == 2 ? 76.0f : 0.0f) + (show_clear ? 40.0f : 0.0f);
	case XENT_CONTROL_TEXT_INPUT   : return show_clear ? 30.0f : 0.0f;
	case XENT_CONTROL_PASSWORD_BOX : return show_reveal ? 30.0f : 0.0f;
	default                        : return 0.0f;
	}
}

static float tb_calc_text_max_w(FluxTextBoxInputData *tb, XentControlType ct) {
	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);

	FluxNodeData *nd       = flux_node_store_get(tb->store, tb->node);
	float         reserved = tb_reserved_trailing_w(tb, ct, nd);
	return rect.width - reserved - 10.0f - 6.0f;
}

static void tb_set_cursor(FluxTextBoxInputData *tb, uint32_t pos, bool extend_selection) {
	tb->base.cursor_position = pos;
	if (extend_selection) {
		tb->base.selection_end = pos;
		return;
	}
	tb->base.selection_start = pos;
	tb->base.selection_end   = pos;
}

static void tb_enter_number_step(FluxTextBoxInputData *tb, double change) {
	nb_step_value(tb, change);
	tb->nb->stepping         = true;
	tb->base.readonly        = true;
	tb->base.selection_start = 0;
	tb->base.selection_end   = 0;
	tb->base.cursor_position = 0;
}

static void tb_leave_number_step(FluxTextBoxInputData *tb) {
	if (!nb_is_number_box(tb) || !tb->nb->stepping) return;
	tb->nb->stepping  = false;
	tb->base.readonly = false;
}

static void tb_move_horizontal(FluxTextBoxInputData *tb, bool forward, bool ctrl, bool shift) {
	uint32_t pos = tb->base.cursor_position;
	if (forward)
		pos = pos < tb->buf_len
		      ? (ctrl ? tb_word_end(tb->buffer, tb->buf_len, pos) : tb_utf8_next(tb->buffer, tb->buf_len, pos))
		      : tb->buf_len;
	else
		pos = pos > 0
		      ? (ctrl ? tb_word_start(tb->buffer, tb->buf_len, pos) : tb_utf8_prev(tb->buffer, tb->buf_len, pos))
		      : 0;
	tb_set_cursor(tb, pos, shift);
	tb_update_scroll(tb);
}

static void tb_move_to_edge(FluxTextBoxInputData *tb, bool end, bool shift) {
	tb_set_cursor(tb, end ? tb->buf_len : 0, shift);
	tb_update_scroll(tb);
}

static void tb_delete_selection_or_cursor(FluxTextBoxInputData *tb, bool backward) {
	if (tb_has_selection(tb)) {
		tb_delete_selection(tb);
		return;
	}
	if (backward && tb->base.cursor_position > 0) {
		uint32_t prev = tb_utf8_prev(tb->buffer, tb->buf_len, tb->base.cursor_position);
		tb_delete_range(tb, prev, tb->base.cursor_position);
		tb_set_cursor(tb, prev, false);
		return;
	}
	if (!backward && tb->base.cursor_position < tb->buf_len) {
		uint32_t next = tb_utf8_next(tb->buffer, tb->buf_len, tb->base.cursor_position);
		tb_delete_range(tb, tb->base.cursor_position, next);
	}
}

static void tb_delete_key(FluxTextBoxInputData *tb, bool backward) {
	if (tb->base.readonly) return;
	tb_push_undo(tb);
	tb->last_op_was_typing = false;
	tb_delete_selection_or_cursor(tb, backward);
	tb_update_scroll(tb);
	tb_notify_change(tb);
}

static bool tb_handle_number_key(FluxTextBoxInputData *tb, unsigned int vk) {
	if (!nb_is_number_box(tb)) return false;
	switch (vk) {
	case VK_UP    : tb_enter_number_step(tb, tb->nb->small_change); return true;
	case VK_DOWN  : tb_enter_number_step(tb, -tb->nb->small_change); return true;
	case VK_PRIOR : tb_enter_number_step(tb, tb->nb->large_change); return true;
	case VK_NEXT  : tb_enter_number_step(tb, -tb->nb->large_change); return true;
	case VK_RETURN :
		tb_leave_number_step(tb);
		nb_validate_input(tb);
		return true;
	case VK_ESCAPE :
		tb_leave_number_step(tb);
		nb_update_text_to_value(tb);
		return true;
	default : return false;
	}
}

static void tb_cut_selection(FluxTextBoxInputData *tb) {
	if (!tb_has_selection(tb) || tb->base.readonly) return;
	tb_perform_copy(tb);
	tb_push_undo(tb);
	tb->last_op_was_typing = false;
	tb_delete_selection(tb);
	tb_update_scroll(tb);
	tb_notify_change(tb);
}

static bool tb_insert_would_fit(FluxTextBoxInputData *tb, int u8len) {
	if (tb->base.max_length == 0) return true;
	uint32_t after_len = tb->buf_len + ( uint32_t ) u8len;
	if (tb_has_selection(tb)) {
		uint32_t s
		  = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
		uint32_t e
		  = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
		after_len -= (e - s);
	}
	return after_len <= tb->base.max_length;
}

static void tb_prepare_typing_undo(FluxTextBoxInputData *tb, wchar_t ch) {
	ULONGLONG now_ms   = GetTickCount64();
	bool      is_space = (ch == ' ' || ch == '\t');
	if (tb->last_op_was_typing && !is_space && (now_ms - tb->last_typing_time < TB_TYPING_MERGE_MS)) {
		tb->last_typing_time = now_ms;
		return;
	}
	tb_push_undo(tb);
	tb->last_typing_time   = now_ms;
	tb->last_op_was_typing = true;
}

static bool tb_use_password_mask(FluxTextBoxInputData *tb, XentControlType ct) {
	return ct == XENT_CONTROL_PASSWORD_BOX && tb->buf_len > 0 && !xent_get_semantic_checked(tb->ctx, tb->node);
}

static char const *tb_password_mask_text(FluxTextBoxInputData *tb, char stack [TB_MASK_STACK_BYTES], char **heap) {
	*heap = NULL;
	if (tb->buf_len > (UINT32_MAX - 1u) / 3u) return "";
	size_t bytes = ( size_t ) tb->buf_len * 3u + 1u;
	char  *dst   = bytes <= TB_MASK_STACK_BYTES ? stack : ( char * ) malloc(bytes);
	if (!dst) return "";

	pb_build_mask(tb->buffer, tb->buf_len, dst, ( uint32_t ) bytes);
	if (dst != stack) *heap = dst;
	return dst;
}

static uint32_t
tb_hit_test_byte_pos(FluxTextBoxInputData *tb, FluxTextRenderer *tr, XentControlType ct, float local_x) {
	float         max_w    = tb_calc_text_max_w(tb, ct);
	float         text_x   = local_x - 10.0f + tb->base.scroll_offset_x;
	FluxTextStyle ts       = tb_make_style(tb);

	bool          use_mask = tb_use_password_mask(tb, ct);
	char          mask_buf [TB_MASK_STACK_BYTES];
	char         *mask_heap = NULL;
	char const   *hit_text  = tb->buffer;
	if (use_mask) hit_text = tb_password_mask_text(tb, mask_buf, &mask_heap);

	FluxTextHitTestQuery query = {
	  {tr, hit_text, &ts, max_w + tb->base.scroll_offset_x},
      text_x, 0.0f
    };
	int u16_pos = flux_text_hit_test(&query);
	if (!use_mask) return tb_utf16_to_byte_offset(tb->buffer, tb->buf_len, ( uint32_t ) u16_pos);

	uint32_t mask_len  = ( uint32_t ) strlen(hit_text);
	uint32_t mask_byte = tb_utf16_to_byte_offset(hit_text, mask_len, ( uint32_t ) u16_pos);
	uint32_t original  = pb_mask_offset_to_original(tb->buffer, tb->buf_len, mask_byte);
	free(mask_heap);
	return original;
}

static void tb_apply_pointer_position(FluxTextBoxInputData *tb, uint32_t byte_pos) {
	if (tb->dragging) {
		tb->base.selection_start = tb->drag_anchor;
		tb->base.selection_end   = byte_pos;
		tb->base.cursor_position = byte_pos;
		return;
	}
	tb_set_cursor(tb, byte_pos, false);
}

static void tb_clear_text(FluxTextBoxInputData *tb) {
	tb_leave_number_step(tb);
	tb_push_undo(tb);
	tb->last_op_was_typing   = false;
	tb->buffer [0]           = '\0';
	tb->buf_len              = 0;
	tb->base.content         = tb->buffer;
	tb->base.cursor_position = 0;
	tb->base.selection_start = 0;
	tb->base.selection_end   = 0;
	tb_update_scroll(tb);
	tb_notify_change(tb);
}

static bool tb_handle_delete_button(FluxTextBoxInputData *tb, XentControlType ct, float local_x) {
	if (ct != XENT_CONTROL_TEXT_INPUT && ct != XENT_CONTROL_NUMBER_BOX) return false;

	float         del_w            = (ct == XENT_CONTROL_NUMBER_BOX) ? 40.0f : 30.0f;
	float         del_right_offset = (ct == XENT_CONTROL_NUMBER_BOX && tb->nb->spin_placement == 2) ? 76.0f : 0.0f;
	FluxNodeData *nd               = flux_node_store_get(tb->store, tb->node);
	bool          show_del         = tb->buffer && tb->buf_len > 0 && nd && nd->state.focused && !tb->base.readonly;
	if (!show_del) return false;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	float delete_x = rect.width - del_w - del_right_offset;
	if (local_x < delete_x || local_x >= delete_x + del_w) return false;

	tb_clear_text(tb);
	return true;
}

static bool tb_handle_password_reveal(FluxTextBoxInputData *tb, XentControlType ct, float local_x) {
	if (ct != XENT_CONTROL_PASSWORD_BOX) return false;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	FluxNodeData *nd       = flux_node_store_get(tb->store, tb->node);
	bool          show_rev = tb->buffer && tb->buf_len > 0 && nd && nd->state.focused;
	if (!show_rev || local_x < rect.width - 30.0f) return false;

	xent_set_semantic_checked(tb->ctx, tb->node, 1);
	return true;
}

static void tb_prepare_number_pointer(FluxTextBoxInputData *tb) {
	if (!nb_is_number_box(tb)) return;
	tb->nb->focus_from_click = true;
	tb_leave_number_step(tb);
}

static void tb_apply_click_selection(FluxTextBoxInputData *tb, uint32_t byte_pos, int click_count) {
	if (click_count >= 3) {
		tb->base.selection_start = 0;
		tb->base.selection_end   = tb->buf_len;
		tb->base.cursor_position = tb->buf_len;
		tb->dragging             = false;
		return;
	}
	if (click_count == 2) {
		uint32_t ws              = tb_word_start(tb->buffer, tb->buf_len, byte_pos);
		uint32_t we              = tb_word_end(tb->buffer, tb->buf_len, byte_pos);
		tb->base.selection_start = ws;
		tb->base.selection_end   = we;
		tb->base.cursor_position = we;
		tb->dragging             = false;
		return;
	}

	bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	tb_set_cursor(tb, byte_pos, shift);
	tb->dragging    = true;
	tb->drag_anchor = tb->base.selection_start;
}

static bool tb_handle_navigation_key(FluxTextBoxInputData *tb, unsigned int vk, bool shift, bool ctrl) {
	switch (vk) {
	case VK_LEFT  : tb_move_horizontal(tb, false, ctrl, shift); return true;
	case VK_RIGHT : tb_move_horizontal(tb, true, ctrl, shift); return true;
	case VK_HOME  : tb_move_to_edge(tb, false, shift); return true;
	case VK_END   : tb_move_to_edge(tb, true, shift); return true;
	default       : return false;
	}
}

static bool tb_handle_edit_key(FluxTextBoxInputData *tb, unsigned int vk) {
	switch (vk) {
	case VK_BACK   : tb_delete_key(tb, true); return true;
	case VK_DELETE : tb_delete_key(tb, false); return true;
	default        : return false;
	}
}

static bool tb_is_submit_key(unsigned int vk) {
	static unsigned int const keys [] = {VK_UP, VK_DOWN, VK_PRIOR, VK_NEXT, VK_RETURN, VK_ESCAPE};
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys [0]); i++)
		if (keys [i] == vk) return true;
	return false;
}

static void tb_submit_if_needed(FluxTextBoxInputData *tb, unsigned int vk) {
	if (vk == VK_RETURN && tb->base.on_submit) tb->base.on_submit(tb->base.on_submit_ctx);
}

static bool tb_handle_submit_key(FluxTextBoxInputData *tb, unsigned int vk) {
	if (!tb_is_submit_key(vk)) return false;
	if (tb_handle_number_key(tb, vk)) return true;
	tb_submit_if_needed(tb, vk);
	return true;
}

static bool tb_handle_clipboard_shortcut(FluxTextBoxInputData *tb, unsigned int vk, bool ctrl) {
	if (!ctrl) return false;

	switch (vk) {
	case 'A' :
		tb->base.selection_start = 0;
		tb->base.selection_end   = tb->buf_len;
		tb->base.cursor_position = tb->buf_len;
		return true;
	case 'C' : tb_perform_copy(tb); return true;
	case 'X' : tb_cut_selection(tb); return true;
	case 'V' :
		if (!tb->base.readonly) tb_perform_paste(tb);
		return true;
	default : return false;
	}
}

static bool tb_handle_history_shortcut(FluxTextBoxInputData *tb, unsigned int vk, bool ctrl) {
	if (!ctrl) return false;

	switch (vk) {
	case 'Z' : tb_undo(tb); return true;
	case 'Y' : tb_redo(tb); return true;
	default  : return false;
	}
}

void tb_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return;
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled) return;

	uint32_t old_cursor = tb->base.cursor_position;
	bool     shift      = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	bool     ctrl       = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (tb_handle_history_shortcut(tb, vk, ctrl)) return;
	if (!tb_handle_navigation_key(tb, vk, shift, ctrl)
		&& !tb_handle_edit_key(tb, vk)
		&& !tb_handle_submit_key(tb, vk)
		&& !tb_handle_clipboard_shortcut(tb, vk, ctrl))
		return;

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

void tb_on_char(void *ctx, wchar_t ch) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled || tb->base.readonly) return;

	tb_leave_number_step(tb);
	if (ch < 0x20 && ch != '\t') return;
	if (ch == 0x7f) return;
	if (ch == '\t' && !tb->base.multiline) return;

	char u8 [4];
	int  u8len = 0;
	if (!tb_encode_utf8(&tb->high_surrogate, ch, u8, &u8len)) return;
	if (!tb_insert_would_fit(tb, u8len)) return;

	tb_prepare_typing_undo(tb, ch);
	if (tb_has_selection(tb)) tb_delete_selection(tb);

	uint32_t old_cursor = tb->base.cursor_position;
	tb_insert_utf8(tb, old_cursor, u8, ( uint32_t ) u8len);
	tb_set_cursor(tb, old_cursor + ( uint32_t ) u8len, false);

	tb_update_scroll(tb);
	tb_notify_change(tb);

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

void tb_on_pointer_move(void *ctx, float local_x, float local_y) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxTextRenderer     *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tb->base.enabled || !tr) return;
	( void ) local_y;

	XentControlType ct       = xent_get_control_type(tb->ctx, tb->node);
	uint32_t        byte_pos = tb_hit_test_byte_pos(tb, tr, ct, local_x);
	tb_apply_pointer_position(tb, byte_pos);
}

void tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxTextRenderer     *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tb->base.enabled || !tr) return;
	( void ) local_y;

	uint32_t        old_cursor = tb->base.cursor_position;
	XentControlType ct         = xent_get_control_type(tb->ctx, tb->node);

	if (tb_handle_delete_button(tb, ct, local_x)) return;
	if (tb_handle_password_reveal(tb, ct, local_x)) return;

	tb_prepare_number_pointer(tb);

	uint32_t byte_pos = tb_hit_test_byte_pos(tb, tr, ct, local_x);
	tb_apply_click_selection(tb, byte_pos, click_count);

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

void tb_on_ime_composition(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;

	if (!text || len == 0) {
		tb_update_ime_position(tb);
		tb->base.composition_text   = NULL;
		tb->base.composition_length = 0;
		tb->base.composition_cursor = 0;
		if (tb->ime_buf) {
			free(tb->ime_buf);
			tb->ime_buf = NULL;
		}
		tb->ime_buf_cap = 0;
		return;
	}

	if (tb->base.composition_text == NULL && tb_has_selection(tb)) {
		tb_push_undo(tb);
		tb->last_op_was_typing = false;
		tb_delete_selection(tb);
		tb_notify_change(tb);
	}

	uint32_t needed = len + 1;
	if (needed > tb->ime_buf_cap) {
		free(tb->ime_buf);
		tb->ime_buf_cap = needed;
		tb->ime_buf     = ( wchar_t * ) malloc(needed * sizeof(wchar_t));
	}
	if (tb->ime_buf) {
		memcpy(tb->ime_buf, text, len * sizeof(wchar_t));
		tb->ime_buf [len] = 0;
	}
	tb->base.composition_text   = tb->ime_buf;
	tb->base.composition_length = len;
	tb->base.composition_cursor = cursor;

	tb_update_ime_position(tb);
}

static void tb_context_add_items(FluxTextBoxInputData *tb, HMENU menu) {
	bool has_sel   = tb_has_selection(tb);
	bool can_paste = IsClipboardFormatAvailable(CF_UNICODETEXT);

	if (!tb->base.readonly) AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_CUT, L"Cut\tCtrl+X");
	AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_COPY, L"Copy\tCtrl+C");
	if (!tb->base.readonly) AppendMenuW(menu, MF_STRING | (can_paste ? 0 : MF_GRAYED), TB_CM_PASTE, L"Paste\tCtrl+V");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TB_CM_SELALL, L"Select All\tCtrl+A");
}

static int tb_context_track(HWND hwnd, HMENU menu) {
	POINT pt;
	GetCursorPos(&pt);
	return ( int ) TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
}

static void tb_select_all(FluxTextBoxInputData *tb) {
	tb->base.selection_start = 0;
	tb->base.selection_end   = tb->buf_len;
	tb->base.cursor_position = tb->buf_len;
}

static void tb_context_dispatch(FluxTextBoxInputData *tb, int cmd) {
	switch (cmd) {
	case TB_CM_CUT : tb_cut_selection(tb); return;
	case TB_CM_COPY :
		if (tb_has_selection(tb)) tb_perform_copy(tb);
		return;
	case TB_CM_PASTE :
		if (!tb->base.readonly) tb_perform_paste(tb);
		return;
	case TB_CM_SELALL : tb_select_all(tb); return;
	default           : return;
	}
}

void tb_on_context_menu(void *ctx, float x, float y) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled) return;
	FluxWindow *window = tb->app ? flux_app_get_window(tb->app) : NULL;
	if (!window) return;
	( void ) x;
	( void ) y;

	HMENU menu = CreatePopupMenu();
	if (!menu) return;

	tb_context_add_items(tb, menu);
	int cmd = tb_context_track(flux_window_hwnd(window), menu);
	DestroyMenu(menu);
	tb_context_dispatch(tb, cmd);
}

void tb_on_focus(void *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxNodeData         *nd = flux_node_store_get(tb->store, tb->node);
	if (nd) nd->state.focused = 1;
	tb_update_ime_position(tb);

	if (!nb_is_number_box(tb)) return;
	if (tb->nb->focus_from_click) {
		tb->nb->focus_from_click = false;
		tb->base.selection_start = 0;
		tb->base.selection_end   = tb->buf_len;
		tb->base.cursor_position = tb->buf_len;
		return;
	}

	tb->nb->stepping         = true;
	tb->base.readonly        = true;
	tb->base.selection_start = 0;
	tb->base.selection_end   = 0;
	tb->base.cursor_position = 0;
}

void tb_on_blur(void *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxNodeData         *nd = flux_node_store_get(tb->store, tb->node);
	if (nd) nd->state.focused = 0;
	tb->base.selection_start    = tb->base.cursor_position;
	tb->base.selection_end      = tb->base.cursor_position;
	tb->base.composition_text   = NULL;
	tb->base.composition_length = 0;
	tb->base.composition_cursor = 0;
	tb->dragging                = false;

	if (xent_get_control_type(tb->ctx, tb->node) == XENT_CONTROL_PASSWORD_BOX)
		xent_set_semantic_checked(tb->ctx, tb->node, 0);

	if (nb_is_number_box(tb)) nb_validate_input(tb);
}
