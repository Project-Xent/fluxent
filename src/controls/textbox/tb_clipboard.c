/**
 * @file tb_clipboard.c
 * @brief Clipboard operations for TextBox (copy, paste).
 */
#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

void tb_perform_copy(FluxTextBoxInputData *tb) {
	if (!tb_has_selection(tb)) return;
	uint32_t s = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	uint32_t e = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	uint32_t sel_len = e - s;
	int      wlen    = MultiByteToWideChar(CP_UTF8, 0, tb->buffer + s, ( int ) sel_len, NULL, 0);
		if (wlen > 0 && OpenClipboard(NULL)) {
			EmptyClipboard();
			HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
				if (hg) {
					wchar_t *p = ( wchar_t * ) GlobalLock(hg);
					MultiByteToWideChar(CP_UTF8, 0, tb->buffer + s, ( int ) sel_len, p, wlen);
					p [wlen] = 0;
					GlobalUnlock(hg);
					SetClipboardData(CF_UNICODETEXT, hg);
				}
			CloseClipboard();
		}
}

void tb_perform_paste(FluxTextBoxInputData *tb) {
	if (tb->base.readonly) return;
	if (!OpenClipboard(NULL)) return;
	HANDLE hg = GetClipboardData(CF_UNICODETEXT);
		if (!hg) {
			CloseClipboard();
			return;
		}
	wchar_t const *wclip = ( wchar_t const * ) GlobalLock(hg);
		if (!wclip) {
			CloseClipboard();
			return;
		}

	tb_push_undo(tb);
	tb->last_op_was_typing = false;
	if (tb_has_selection(tb)) tb_delete_selection(tb);

	int      wclip_len = ( int ) wcslen(wclip);
	/* Strip newlines for single-line */
	wchar_t  filtered_stack [256];
	wchar_t *filtered = filtered_stack;
		if (!tb->base.multiline) {
			if (wclip_len + 1 > 256) filtered = ( wchar_t * ) malloc((wclip_len + 1) * sizeof(wchar_t));
			int fi = 0;
			for (int i = 0; i < wclip_len; i++)
				if (wclip [i] != '\n' && wclip [i] != '\r') filtered [fi++] = wclip [i];
			filtered [fi] = 0;
			wclip         = filtered;
			wclip_len     = fi;
		}

	int u8len = WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, NULL, 0, NULL, NULL);
		if (u8len > 0) {
			if (tb->base.max_length > 0 && tb->buf_len + ( uint32_t ) u8len > tb->base.max_length)
				u8len = ( int ) (tb->base.max_length > tb->buf_len ? tb->base.max_length - tb->buf_len : 0);
				if (u8len > 0) {
					char  u8_stack [512];
					char *u8buf = u8_stack;
					if (u8len + 1 > 512) u8buf = ( char * ) malloc(u8len + 1);
					WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, u8buf, u8len, NULL, NULL);
					u8buf [u8len] = 0;
					tb_insert_utf8(tb, tb->base.cursor_position, u8buf, ( uint32_t ) u8len);
					tb->base.cursor_position += ( uint32_t ) u8len;
					tb->base.selection_start  = tb->base.cursor_position;
					tb->base.selection_end    = tb->base.cursor_position;
					if (u8buf != u8_stack) free(u8buf);
				}
		}

	GlobalUnlock(hg);
	CloseClipboard();
	if (!tb->base.multiline && filtered != filtered_stack) free(filtered);

	tb_update_scroll(tb);
	tb_notify_change(tb);
}
