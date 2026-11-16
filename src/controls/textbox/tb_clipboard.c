#include "tb_internal.h"

#include "fluxent/flux_data.h"
#include <stdlib.h>
#include <string.h>

static bool tb_copy_selection_range(FluxTextBoxInputData *tb, uint32_t *start, uint32_t *len) {
	if (!tb_has_selection(tb)) return false;
	uint32_t s = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	uint32_t e = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	*start     = s;
	*len       = e - s;
	return *len > 0;
}

void tb_perform_copy(FluxTextBoxInputData *tb) {
	uint32_t start = 0;
	uint32_t len   = 0;
	if (!tb_copy_selection_range(tb, &start, &len)) return;
	char const *content = tb_sync_content(tb);
	( void ) flux_clipboard_settext(content + start, len);
}

static uint32_t tb_strip_newlines(char *text) {
	uint32_t out = 0;
	for (char const *p = text; *p; p++)
		if (*p != '\n' && *p != '\r') text [out++] = *p;
	text [out] = '\0';
	return out;
}

static uint32_t tb_paste_limit(FluxTextBoxInputData const *tb, char const *text, uint32_t len) {
	if (tb->base.max_length == 0) return len;
	if (tb->buf_len >= tb->base.max_length) return 0;
	uint32_t room = tb->base.max_length - tb->buf_len;
	if (len <= room) return len;
	while (room > 0 && (( unsigned char ) text [room] & 0xc0u) == 0x80u) room--;
	return room;
}

static void tb_paste_text(FluxTextBoxInputData *tb, char *text) {
	uint32_t len = tb->base.multiline ? ( uint32_t ) strlen(text) : tb_strip_newlines(text);
	len          = tb_paste_limit(tb, text, len);
	tb_push_undo(tb);
	tb->last_op_was_typing = false;
	if (tb_has_selection(tb)) tb_delete_selection(tb);
	if (len == 0) return;
	tb_insert_utf8(tb, tb->base.cursor_position, text, len);
	tb->base.cursor_position += len;
	tb->base.selection_start  = tb->base.cursor_position;
	tb->base.selection_end    = tb->base.cursor_position;
}

void tb_perform_paste(FluxTextBoxInputData *tb) {
	if (tb->base.readonly) return;
	char *text = flux_clipboard_text();
	if (!text) return;
	tb_paste_text(tb, text);
	free(text);
	tb_update_scroll(tb);
	tb_update_ime_position(tb);
	tb_notify_change(tb);
}
