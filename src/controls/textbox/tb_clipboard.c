#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct TbClipboardText {
	HANDLE         handle;
	wchar_t const *text;
} TbClipboardText;

static bool tb_clipboard_lock_text(TbClipboardText *clip) {
	clip->handle = GetClipboardData(CF_UNICODETEXT);
	if (!clip->handle) return false;
	clip->text = ( wchar_t const * ) GlobalLock(clip->handle);
	return clip->text != NULL;
}

static void tb_clipboard_unlock_text(TbClipboardText *clip) {
	if (clip->text) GlobalUnlock(clip->handle);
	clip->handle = NULL;
	clip->text   = NULL;
}

static bool tb_copy_selection_range(FluxTextBoxInputData *tb, uint32_t *start, uint32_t *len) {
	if (!tb_has_selection(tb)) return false;
	uint32_t s = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	uint32_t e = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	*start     = s;
	*len       = e - s;
	return *len > 0;
}

static void tb_set_clipboard_utf16(char const *utf8, uint32_t len) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, ( int ) len, NULL, 0);
	if (wlen <= 0 || !OpenClipboard(NULL)) return;

	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (( size_t ) wlen + 1u) * sizeof(wchar_t));
	if (hg) {
		wchar_t *p = ( wchar_t * ) GlobalLock(hg);
		MultiByteToWideChar(CP_UTF8, 0, utf8, ( int ) len, p, wlen);
		p [wlen] = 0;
		GlobalUnlock(hg);
		SetClipboardData(CF_UNICODETEXT, hg);
	}
	CloseClipboard();
}

void tb_perform_copy(FluxTextBoxInputData *tb) {
	uint32_t start = 0;
	uint32_t len   = 0;
	if (tb_copy_selection_range(tb, &start, &len)) tb_set_clipboard_utf16(tb->buffer + start, len);
}

static wchar_t const *tb_filtered_clip_text(
  FluxTextBoxInputData *tb, wchar_t const *wclip, int *wclip_len, wchar_t **heap, wchar_t stack [256]
) {
	*heap      = NULL;
	*wclip_len = ( int ) wcslen(wclip);
	if (tb->base.multiline) return wclip;

	if (*wclip_len + 1 > 256) {
		*heap = ( wchar_t * ) malloc((( size_t ) *wclip_len + 1u) * sizeof(wchar_t));
		if (!*heap) return L"";
	}

	wchar_t *filtered = *heap ? *heap : stack;
	int      out      = 0;
	for (int i = 0; i < *wclip_len; i++)
		if (wclip [i] != L'\n' && wclip [i] != L'\r') filtered [out++] = wclip [i];
	filtered [out] = 0;
	*wclip_len     = out;
	return filtered;
}

static int tb_clipboard_u8_limit(FluxTextBoxInputData *tb, int u8len) {
	if (tb->base.max_length == 0) return u8len;
	if (tb->buf_len >= tb->base.max_length) return 0;

	uint32_t room = tb->base.max_length - tb->buf_len;
	return ( uint32_t ) u8len > room ? ( int ) room : u8len;
}

static void tb_insert_clip_utf16(FluxTextBoxInputData *tb, wchar_t const *wclip, int wclip_len) {
	int u8len = WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, NULL, 0, NULL, NULL);
	u8len     = u8len > 0 ? tb_clipboard_u8_limit(tb, u8len) : 0;
	if (u8len <= 0) return;

	char  u8_stack [512];
	char *u8buf = u8len + 1 > 512 ? ( char * ) malloc(( size_t ) u8len + 1u) : u8_stack;
	if (!u8buf) return;

	WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, u8buf, u8len, NULL, NULL);
	u8buf [u8len] = 0;
	tb_insert_utf8(tb, tb->base.cursor_position, u8buf, ( uint32_t ) u8len);
	tb->base.cursor_position += ( uint32_t ) u8len;
	tb->base.selection_start  = tb->base.cursor_position;
	tb->base.selection_end    = tb->base.cursor_position;
	if (u8buf != u8_stack) free(u8buf);
}

static void tb_paste_locked_text(FluxTextBoxInputData *tb, wchar_t const *wclip) {
	wchar_t  filtered_stack [256];
	wchar_t *filtered_heap = NULL;
	int      wclip_len     = 0;
	wclip                  = tb_filtered_clip_text(tb, wclip, &wclip_len, &filtered_heap, filtered_stack);

	tb_push_undo(tb);
	tb->last_op_was_typing = false;
	if (tb_has_selection(tb)) tb_delete_selection(tb);
	tb_insert_clip_utf16(tb, wclip, wclip_len);
	free(filtered_heap);
}

void tb_perform_paste(FluxTextBoxInputData *tb) {
	if (tb->base.readonly) return;
	if (!OpenClipboard(NULL)) return;

	TbClipboardText clip = {0};
	if (tb_clipboard_lock_text(&clip)) tb_paste_locked_text(tb, clip.text);

	tb_clipboard_unlock_text(&clip);
	CloseClipboard();

	tb_update_scroll(tb);
	tb_notify_change(tb);
}
