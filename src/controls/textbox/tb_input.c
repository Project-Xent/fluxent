/**
 * @file tb_input.c
 * @brief Input event handlers for TextBox (keyboard, pointer, IME, focus/blur).
 */
#include "tb_internal.h"
#include "fluxent/fluxent.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/* Context menu command IDs */
#define TB_CM_CUT    1
#define TB_CM_COPY   2
#define TB_CM_PASTE  3
#define TB_CM_SELALL 4

/* ═══════════════════════════════════════════════════════════════════════
   Keyboard handler
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return;
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled) return;
	uint32_t old_cursor = tb->base.cursor_position;

	bool     shift      = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	bool     ctrl       = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

		switch (vk) {
		case VK_LEFT :
				if (tb->base.cursor_position > 0) {
					uint32_t np;
					if (ctrl) np = tb_word_start(tb->buffer, tb->base.cursor_position);
					else np = tb_utf8_prev(tb->buffer, tb->base.cursor_position);
					tb->base.cursor_position = np;
						if (!shift) {
							tb->base.selection_start = np;
							tb->base.selection_end   = np;
						}
						else { tb->base.selection_end = np; }
				}
				else if (!shift) {
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
				}
			tb_update_scroll(tb);
			break;

			case VK_RIGHT : {
				uint32_t len = tb->buf_len;
					if (tb->base.cursor_position < len) {
						uint32_t np;
						if (ctrl) np = tb_word_end(tb->buffer, tb->base.cursor_position);
						else np = tb_utf8_next(tb->buffer, tb->base.cursor_position);
						tb->base.cursor_position = np;
							if (!shift) {
								tb->base.selection_start = np;
								tb->base.selection_end   = np;
							}
							else { tb->base.selection_end = np; }
					}
					else if (!shift) {
						tb->base.selection_start = len;
						tb->base.selection_end   = len;
					}
				tb_update_scroll(tb);
				break;
			}

		case VK_HOME :
			tb->base.cursor_position = 0;
				if (!shift) {
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
				}
				else { tb->base.selection_end = 0; }
			tb_update_scroll(tb);
			break;

		case VK_END :
			tb->base.cursor_position = tb->buf_len;
				if (!shift) {
					tb->base.selection_start = tb->buf_len;
					tb->base.selection_end   = tb->buf_len;
				}
				else { tb->base.selection_end = tb->buf_len; }
			tb_update_scroll(tb);
			break;

		case VK_BACK :
			if (tb->base.readonly) break;
			tb_push_undo(tb);
			tb->last_op_was_typing = false;
				if (tb_has_selection(tb)) { tb_delete_selection(tb); }
				else if (tb->base.cursor_position > 0) {
					uint32_t prev = tb_utf8_prev(tb->buffer, tb->base.cursor_position);
					tb_delete_range(tb, prev, tb->base.cursor_position);
					tb->base.cursor_position = prev;
					tb->base.selection_start = prev;
					tb->base.selection_end   = prev;
				}
			tb_update_scroll(tb);
			tb_notify_change(tb);
			break;

		case VK_DELETE :
			if (tb->base.readonly) break;
			tb_push_undo(tb);
			tb->last_op_was_typing = false;
				if (tb_has_selection(tb)) { tb_delete_selection(tb); }
				else if (tb->base.cursor_position < tb->buf_len) {
					uint32_t next = tb_utf8_next(tb->buffer, tb->base.cursor_position);
					tb_delete_range(tb, tb->base.cursor_position, next);
				}
			tb_update_scroll(tb);
			tb_notify_change(tb);
			break;

		/* ---- NumberBox keyboard shortcuts (WinUI OnNumberBoxKeyDown) ---- */
		case VK_UP :
				if (nb_is_number_box(tb)) {
					nb_step_value(tb, tb->nb_small_change);
					tb->nb_stepping          = true;
					tb->base.readonly        = true;
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
					tb->base.cursor_position = 0;
					return; /* consume — don't let textbox handle it */
				}
			break;
		case VK_DOWN :
				if (nb_is_number_box(tb)) {
					nb_step_value(tb, -tb->nb_small_change);
					tb->nb_stepping          = true;
					tb->base.readonly        = true;
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
					tb->base.cursor_position = 0;
					return;
				}
			break;
		case VK_PRIOR : /* PageUp */
				if (nb_is_number_box(tb)) {
					nb_step_value(tb, tb->nb_large_change);
					tb->nb_stepping          = true;
					tb->base.readonly        = true;
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
					tb->base.cursor_position = 0;
					return;
				}
			break;
		case VK_NEXT : /* PageDown */
				if (nb_is_number_box(tb)) {
					nb_step_value(tb, -tb->nb_large_change);
					tb->nb_stepping          = true;
					tb->base.readonly        = true;
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
					tb->base.cursor_position = 0;
					return;
				}
			break;

		case VK_RETURN :
				if (nb_is_number_box(tb)) {
					tb->nb_stepping   = false;
					tb->base.readonly = false;
					nb_validate_input(tb);
					return;
				}
			if (tb->base.on_submit) tb->base.on_submit(tb->base.on_submit_ctx);
			break;

		case VK_ESCAPE :
				if (nb_is_number_box(tb)) {
					tb->nb_stepping   = false;
					tb->base.readonly = false;
					nb_update_text_to_value(tb); /* revert to current Value */
					return;
				}
			break;

		case 'A' :
				if (ctrl) {
					tb->base.selection_start = 0;
					tb->base.selection_end   = tb->buf_len;
					tb->base.cursor_position = tb->buf_len;
				}
			break;

		case 'C' :
			if (ctrl) tb_perform_copy(tb);
			break;

		case 'X' :
				if (ctrl && tb_has_selection(tb) && !tb->base.readonly) {
					tb_perform_copy(tb);
					tb_push_undo(tb);
					tb->last_op_was_typing = false;
					tb_delete_selection(tb);
					tb_update_scroll(tb);
					tb_notify_change(tb);
				}
			break;

		case 'V' :
			if (ctrl && !tb->base.readonly) tb_perform_paste(tb);
			break;

		case 'Z' :
				if (ctrl) {
					tb_undo(tb);
					return;
				}
			break;

		case 'Y' :
				if (ctrl) {
					tb_redo(tb);
					return;
				}
			break;

		default : break;
		}

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

/* ═══════════════════════════════════════════════════════════════════════
   Character input handler
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_char(void *ctx, wchar_t ch) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled) return;
	if (tb->base.readonly) return;

		/* NumberBox: typing a character exits stepping mode → enter edit mode */
		if (nb_is_number_box(tb) && tb->nb_stepping) {
			tb->nb_stepping   = false;
			tb->base.readonly = false;
		}
	uint32_t old_cursor = tb->base.cursor_position;

	/* Filter control characters */
	if (ch < 0x20 && ch != '\t') return;
	if (ch == 0x7f) return; /* DEL */
	if (ch == '\t' && !tb->base.multiline) return;

	/*
	 * Surrogate pair handling for U+10000+ (emoji, CJK Extension B, etc.)
	 * Windows sends two WM_CHAR messages: high surrogate (0xD800–0xDBFF)
	 * followed by low surrogate (0xDC00–0xDFFF).
	 */
	char u8 [4];
	int  u8len = 0;

		if (ch >= 0xd800 && ch <= 0xdbff) {
			/* High surrogate — buffer it, wait for the low half */
			tb->high_surrogate = ch;
			return;
		}

		if (ch >= 0xdc00 && ch <= 0xdfff) {
			if (tb->high_surrogate == 0) return; /* orphan low surrogate */
			/* Combine into a full code point and encode as 4-byte UTF-8 */
			uint32_t cp = 0x10000 + (( uint32_t ) (tb->high_surrogate - 0xd800) << 10) + (( uint32_t ) (ch - 0xdc00));
			tb->high_surrogate = 0;
			u8 [0]             = ( char ) (0xf0 | (cp >> 18));
			u8 [1]             = ( char ) (0x80 | ((cp >> 12) & 0x3f));
			u8 [2]             = ( char ) (0x80 | ((cp >> 6) & 0x3f));
			u8 [3]             = ( char ) (0x80 | (cp & 0x3f));
			u8len              = 4;
		}
		else {
			/* Stale high surrogate without a matching low — discard it */
			tb->high_surrogate = 0;

				if (ch < 0x80) {
					u8 [0] = ( char ) ch;
					u8len  = 1;
				}
				else if (ch < 0x800) {
					u8 [0] = ( char ) (0xc0 | (ch >> 6));
					u8 [1] = ( char ) (0x80 | (ch & 0x3f));
					u8len  = 2;
				}
				else {
					u8 [0] = ( char ) (0xe0 | (ch >> 12));
					u8 [1] = ( char ) (0x80 | ((ch >> 6) & 0x3f));
					u8 [2] = ( char ) (0x80 | (ch & 0x3f));
					u8len  = 3;
				}
		}

		/* max_length check */
		if (tb->base.max_length > 0) {
			uint32_t after_len = tb->buf_len + ( uint32_t ) u8len;
				if (tb_has_selection(tb)) {
					uint32_t s  = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start
					                                                                : tb->base.selection_end;
					uint32_t e  = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start
					                                                                : tb->base.selection_end;
					after_len  -= (e - s);
				}
			if (after_len > tb->base.max_length) return;
		}

	/* Undo grouping: merge consecutive typing within threshold */
	{
		ULONGLONG now_ms   = GetTickCount64();
		bool      is_space = (ch == ' ' || ch == '\t');

			if (tb->last_op_was_typing && !is_space && (now_ms - tb->last_typing_time < TB_TYPING_MERGE_MS)) {
				tb->last_typing_time = now_ms;
			}
			else {
				tb_push_undo(tb);
				tb->last_typing_time   = now_ms;
				tb->last_op_was_typing = true;
			}
	}

	if (tb_has_selection(tb)) tb_delete_selection(tb);

	tb_insert_utf8(tb, tb->base.cursor_position, u8, ( uint32_t ) u8len);
	tb->base.cursor_position += ( uint32_t ) u8len;
	tb->base.selection_start  = tb->base.cursor_position;
	tb->base.selection_end    = tb->base.cursor_position;

	tb_update_scroll(tb);
	tb_notify_change(tb);

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

/* ═══════════════════════════════════════════════════════════════════════
   Pointer move (drag selection)
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_pointer_move(void *ctx, float local_x, float local_y) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxTextRenderer     *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tb->base.enabled || !tr) return;
	( void ) local_y;

	float text_x        = local_x - 10.0f; /* FLUX_TEXTBOX_PAD_L */
	text_x             += tb->base.scroll_offset_x;

	FluxTextStyle ts    = tb_make_style(tb);

	XentRect      rect  = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	float           max_w   = rect.width - 10.0f - 6.0f;

	/* TextBox / NumberBox: limit drag hit-test to text column (exclude buttons on right) */
	XentControlType move_ct = xent_get_control_type(tb->ctx, tb->node);
		if (move_ct == XENT_CONTROL_NUMBER_BOX) {
			/* NumberBox: always subtract spin area (if inline) + X button (if visible) */
			float reserved_mv = 0.0f;
			if (tb->nb_spin_placement == 2) reserved_mv += 76.0f;
			bool          has_text_mv_nb = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_mv_nb       = flux_node_store_get(tb->store, tb->node);
			bool          mv_nb_focused  = (nd_mv_nb && nd_mv_nb->state.focused);
			if (has_text_mv_nb && mv_nb_focused && !tb->base.readonly) reserved_mv += 40.0f;
			if (reserved_mv > 0.0f) max_w = (rect.width - reserved_mv) - 10.0f - 6.0f;
		}
		else if (move_ct == XENT_CONTROL_TEXT_INPUT) {
			bool          has_text_mv_tb = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_mv_tb       = flux_node_store_get(tb->store, tb->node);
			bool          mv_tb_focused  = (nd_mv_tb && nd_mv_tb->state.focused);
			if (has_text_mv_tb && mv_tb_focused && !tb->base.readonly) max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
		}

		/* PasswordBox: limit drag hit-test to Column 0 when reveal button is visible */
		if (move_ct == XENT_CONTROL_PASSWORD_BOX) {
			bool          has_text_mv = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_mv       = flux_node_store_get(tb->store, tb->node);
			bool          mv_focused  = (nd_mv && nd_mv->state.focused);
			if (has_text_mv && mv_focused) max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
		}

		/* NumberBox: limit drag hit-test to Column 0 when spin buttons visible */
		if (move_ct == XENT_CONTROL_NUMBER_BOX) {
			if (tb->nb_spin_placement == 2) /* Inline */
				max_w = (rect.width - 76.0f) - 10.0f - 6.0f;
		}

	/* For PasswordBox, hit-test against masked text (unless revealed) */
	bool move_use_mask
	  = (move_ct == XENT_CONTROL_PASSWORD_BOX && tb->buf_len > 0 && !xent_get_semantic_checked(tb->ctx, tb->node));
	char        move_mask [2048];
	char const *hit_text = tb->buffer;
		if (move_use_mask) {
			pb_build_mask(tb->buffer, tb->buf_len, move_mask, sizeof(move_mask));
			hit_text = move_mask;
		}

	int      u16_pos = flux_text_hit_test(tr, hit_text, &ts, max_w + tb->base.scroll_offset_x, text_x, 0.0f);

	uint32_t byte_pos;
		if (move_use_mask) {
			/* Convert mask UTF-16 position → mask byte offset → original byte offset */
			uint32_t mask_byte = tb_utf16_to_byte_offset(hit_text, ( uint32_t ) u16_pos);
			byte_pos           = pb_mask_offset_to_original(tb->buffer, mask_byte);
		}
		else { byte_pos = tb_utf16_to_byte_offset(tb->buffer, ( uint32_t ) u16_pos); }

		if (tb->dragging) {
			/* Drag selection: anchor stays, cursor moves */
			tb->base.selection_start = tb->drag_anchor;
			tb->base.selection_end   = byte_pos;
			tb->base.cursor_position = byte_pos;
		}
		else {
			/* No drag in progress — just position cursor */
			tb->base.cursor_position = byte_pos;
			tb->base.selection_start = byte_pos;
			tb->base.selection_end   = byte_pos;
		}
}

/* ═══════════════════════════════════════════════════════════════════════
   Pointer down (click, double-click, triple-click)
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxTextRenderer     *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tb->base.enabled || !tr) return;

	uint32_t        old_cursor = tb->base.cursor_position;

	XentControlType ct         = xent_get_control_type(tb->ctx, tb->node);

	/* --- TextBox / NumberBox: check if click is on the DeleteButton (X) ---
	   TextBox: Width=30, visible when has_text && focused && !readonly
	   NumberBox: MinWidth=34, visible only when spin buttons NOT inline */
	{
		bool  del_eligible     = false;
		float del_w            = 30.0f;
		float del_right_offset = 0.0f; /* extra offset for spin buttons to the right */
			if (ct == XENT_CONTROL_TEXT_INPUT) { del_eligible = true; }
			else if (ct == XENT_CONTROL_NUMBER_BOX) {
				del_eligible = true;
				del_w        = 40.0f;
				/* When spin inline, X sits left of the 76px spin area */
				if (tb->nb_spin_placement == 2) del_right_offset = 76.0f;
			}
			if (del_eligible) {
				bool          has_text_del = (tb->buffer && tb->buf_len > 0);
				FluxNodeData *nd_del       = flux_node_store_get(tb->store, tb->node);
				bool          del_focused  = (nd_del && nd_del->state.focused);
				bool          show_delete  = has_text_del && del_focused && !tb->base.readonly;
					if (show_delete) {
						XentRect rect = {0};
						xent_get_layout_rect(tb->ctx, tb->node, &rect);
						float delete_x     = rect.width - del_w - del_right_offset;
						float delete_x_end = delete_x + del_w;
							if (local_x >= delete_x && local_x < delete_x_end) {
									/* Clear all text */
									if (nb_is_number_box(tb) && tb->nb_stepping) {
										tb->nb_stepping   = false;
										tb->base.readonly = false;
									}
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
								return;
							}
					}
			}
	}

		/* --- PasswordBox: check if click is on the reveal button --- */
		if (ct == XENT_CONTROL_PASSWORD_BOX) {
			XentRect rect = {0};
			xent_get_layout_rect(tb->ctx, tb->node, &rect);
			/* WinUI: RevealButton visible only when has_text AND focused */
			bool          has_text_pb = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_pb       = flux_node_store_get(tb->store, tb->node);
			bool          pb_focused  = (nd_pb && nd_pb->state.focused);
			bool          show_reveal = has_text_pb && pb_focused;
				if (show_reveal) {
					float reveal_x = rect.width - 30.0f;
						if (local_x >= reveal_x) {
							/* Press-to-reveal: set checked=1 on press (cleared on pointer_up / blur) */
							xent_set_semantic_checked(tb->ctx, tb->node, 1);
							return;
						}
				}
		}

		/* NumberBox spin-button clicks are fully intercepted at the input layer
	       (flux_input_pointer_down) — they never reach here.  Only text-area
	       clicks arrive, so exit stepping mode → enter edit mode. */
		if (nb_is_number_box(tb)) {
			/* Tell the upcoming on_focus call that this focus came from a click,
			   so it should NOT enter stepping mode. */
			tb->nb_focus_from_click = true;
				if (tb->nb_stepping) {
					tb->nb_stepping   = false;
					tb->base.readonly = false;
				}
		}
	( void ) local_y;

	float text_x        = local_x - 10.0f;
	text_x             += tb->base.scroll_offset_x;

	FluxTextStyle ts    = tb_make_style(tb);

	XentRect      rect  = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	float max_w = rect.width - 10.0f - 6.0f;

		/* TextBox / NumberBox: limit hit-test to text column (exclude buttons on right) */
		if (ct == XENT_CONTROL_NUMBER_BOX) {
			float reserved_dn = 0.0f;
			if (tb->nb_spin_placement == 2) reserved_dn += 76.0f;
			bool          has_text_dn = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_dn       = flux_node_store_get(tb->store, tb->node);
			bool          dn_focused  = (nd_dn && nd_dn->state.focused);
			if (has_text_dn && dn_focused && !tb->base.readonly) reserved_dn += 40.0f;
			if (reserved_dn > 0.0f) max_w = (rect.width - reserved_dn) - 10.0f - 6.0f;
		}
		else if (ct == XENT_CONTROL_TEXT_INPUT) {
			bool          has_text_del2 = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_del2       = flux_node_store_get(tb->store, tb->node);
			bool          del2_focused  = (nd_del2 && nd_del2->state.focused);
			if (has_text_del2 && del2_focused && !tb->base.readonly) max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
		}

		/* PasswordBox: limit hit-test to Column 0 when reveal button is visible */
		if (ct == XENT_CONTROL_PASSWORD_BOX) {
			bool          has_text_pb2 = (tb->buffer && tb->buf_len > 0);
			FluxNodeData *nd_pb2       = flux_node_store_get(tb->store, tb->node);
			bool          pb2_focused  = (nd_pb2 && nd_pb2->state.focused);
			if (has_text_pb2 && pb2_focused) max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
		}

		/* NumberBox: limit hit-test to Column 0 when spin buttons visible */
		if (ct == XENT_CONTROL_NUMBER_BOX) {
			if (tb->nb_spin_placement == 2) /* Inline */
				max_w = (rect.width - 76.0f) - 10.0f - 6.0f;
		}

	/* For PasswordBox, hit-test against masked text (unless revealed) */
	bool down_use_mask
	  = (ct == XENT_CONTROL_PASSWORD_BOX && tb->buf_len > 0 && !xent_get_semantic_checked(tb->ctx, tb->node));
	char        down_mask [2048];
	char const *hit_text = tb->buffer;
		if (down_use_mask) {
			pb_build_mask(tb->buffer, tb->buf_len, down_mask, sizeof(down_mask));
			hit_text = down_mask;
		}

	int      u16_pos = flux_text_hit_test(tr, hit_text, &ts, max_w + tb->base.scroll_offset_x, text_x, 0.0f);

	uint32_t byte_pos;
		if (down_use_mask) {
			uint32_t mask_byte = tb_utf16_to_byte_offset(hit_text, ( uint32_t ) u16_pos);
			byte_pos           = pb_mask_offset_to_original(tb->buffer, mask_byte);
		}
		else { byte_pos = tb_utf16_to_byte_offset(tb->buffer, ( uint32_t ) u16_pos); }

		if (click_count >= 3) {
			/* Triple click: select all */
			tb->base.selection_start = 0;
			tb->base.selection_end   = tb->buf_len;
			tb->base.cursor_position = tb->buf_len;
			tb->dragging             = false;
		}
		else if (click_count == 2) {
			/* Double click: select word */
			uint32_t ws              = tb_word_start(tb->buffer, byte_pos);
			uint32_t we              = tb_word_end(tb->buffer, byte_pos);
			tb->base.selection_start = ws;
			tb->base.selection_end   = we;
			tb->base.cursor_position = we;
			tb->dragging             = false;
		}
		else {
			/* Single click: position cursor, begin drag */
			bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				if (shift) {
					/* Shift-click extends selection */
					tb->base.selection_end   = byte_pos;
					tb->base.cursor_position = byte_pos;
				}
				else {
					tb->base.cursor_position = byte_pos;
					tb->base.selection_start = byte_pos;
					tb->base.selection_end   = byte_pos;
				}
			tb->dragging    = true;
			tb->drag_anchor = tb->base.selection_start;
		}

	if (tb->base.cursor_position != old_cursor) tb_update_ime_position(tb);
}

/* ═══════════════════════════════════════════════════════════════════════
   IME composition
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_ime_composition(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;

		if (!text || len == 0) {
			/* End composition — update IME position to final cursor */
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

		/* On first non-empty composition, delete any selected text */
		if (tb->base.composition_text == NULL && tb_has_selection(tb)) {
			tb_push_undo(tb);
			tb_delete_selection(tb);
			tb_notify_change(tb);
		}

	/* Store composition text */
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

	/* Move IME candidate window to follow the composition cursor */
	tb_update_ime_position(tb);
}

/* ═══════════════════════════════════════════════════════════════════════
   Context menu
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_context_menu(void *ctx, float x, float y) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	if (!tb->base.enabled) return;
	FluxWindow *window = tb->app ? flux_app_get_window(tb->app) : NULL;
	if (!window) return;

	HWND hwnd = flux_window_hwnd(window);
	( void ) x;
	( void ) y;

	HMENU menu = CreatePopupMenu();
	if (!menu) return;

	bool has_sel   = tb_has_selection(tb);
	bool can_paste = IsClipboardFormatAvailable(CF_UNICODETEXT);

	if (!tb->base.readonly) AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_CUT, L"Cut\tCtrl+X");
	AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_COPY, L"Copy\tCtrl+C");
	if (!tb->base.readonly) AppendMenuW(menu, MF_STRING | (can_paste ? 0 : MF_GRAYED), TB_CM_PASTE, L"Paste\tCtrl+V");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TB_CM_SELALL, L"Select All\tCtrl+A");

	POINT pt;
	GetCursorPos(&pt);
	int cmd = ( int ) TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
	DestroyMenu(menu);

		switch (cmd) {
		case TB_CM_CUT :
				if (has_sel && !tb->base.readonly) {
					tb_perform_copy(tb);
					tb_push_undo(tb);
					tb_delete_selection(tb);
					tb_update_scroll(tb);
					tb_notify_change(tb);
				}
			break;
		case TB_CM_COPY :
			if (has_sel) tb_perform_copy(tb);
			break;
		case TB_CM_PASTE :
			if (!tb->base.readonly) tb_perform_paste(tb);
			break;
		case TB_CM_SELALL :
			tb->base.selection_start = 0;
			tb->base.selection_end   = tb->buf_len;
			tb->base.cursor_position = tb->buf_len;
			break;
		}
}

/* ═══════════════════════════════════════════════════════════════════════
   Focus / Blur
   ═══════════════════════════════════════════════════════════════════════ */

void tb_on_focus(void *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxNodeData         *nd = flux_node_store_get(tb->store, tb->node);
	if (nd) nd->state.focused = 1;
	tb_update_ime_position(tb);

		/* NumberBox: if focus came from Tab (not a click), enter stepping mode —
	       no selection, no caret. Clicking enters edit mode directly. */
		if (nb_is_number_box(tb)) {
				if (tb->nb_focus_from_click) {
					/* Focus triggered by click → stay in edit mode, select all (WinUI behavior) */
					tb->nb_focus_from_click  = false;
					tb->base.selection_start = 0;
					tb->base.selection_end   = tb->buf_len;
					tb->base.cursor_position = tb->buf_len;
				}
				else {
					/* Focus triggered by Tab → stepping mode */
					tb->nb_stepping          = true;
					tb->base.readonly        = true;
					tb->base.selection_start = 0;
					tb->base.selection_end   = 0;
					tb->base.cursor_position = 0;
				}
		}
}

void tb_on_blur(void *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx;
	FluxNodeData         *nd = flux_node_store_get(tb->store, tb->node);
	if (nd) nd->state.focused = 0;
	tb->base.selection_start    = tb->base.cursor_position;
	tb->base.selection_end      = tb->base.cursor_position;
	/* Clear IME composition */
	tb->base.composition_text   = NULL;
	tb->base.composition_length = 0;
	tb->base.composition_cursor = 0;
	/* Clear drag state */
	tb->dragging                = false;

	/* PasswordBox: clear reveal on blur (press-to-reveal ends when focus lost) */
	if (xent_get_control_type(tb->ctx, tb->node) == XENT_CONTROL_PASSWORD_BOX)
		xent_set_semantic_checked(tb->ctx, tb->node, 0);

	/* NumberBox: validate input on blur (WinUI OnNumberBoxLostFocus) */
	if (nb_is_number_box(tb)) nb_validate_input(tb);
}
