#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct TbUtf8Head {
	uint32_t cp;
	uint32_t len;
} TbUtf8Head;

static bool       tb_utf8_is_cont(uint8_t c) { return (c & 0xc0) == 0x80; }

static TbUtf8Head tb_utf8_head(uint8_t c0) {
	if (c0 < 0x80) return (TbUtf8Head) {c0, 1};
	if (c0 >= 0xc2 && c0 <= 0xdf) return (TbUtf8Head) {c0 & 0x1f, 2};
	if (c0 >= 0xe0 && c0 <= 0xef) return (TbUtf8Head) {c0 & 0x0f, 3};
	if (c0 >= 0xf0 && c0 <= 0xf4) return (TbUtf8Head) {c0 & 0x07, 4};
	return (TbUtf8Head) {c0, 1};
}

static uint32_t tb_utf8_accept(uint32_t *cp_out, uint32_t cp, uint32_t len) {
	if (cp_out) *cp_out = cp;
	return len;
}

static bool tb_utf8_read_tail(char const *s, uint32_t pos, TbUtf8Head *head) {
	for (uint32_t i = 1; i < head->len; i++) {
		uint8_t cx = ( uint8_t ) s [pos + i];
		if (!tb_utf8_is_cont(cx)) return false;
		head->cp = (head->cp << 6) | (cx & 0x3f);
	}
	return true;
}

static bool tb_utf8_scalar_valid(TbUtf8Head head) {
	if (head.len == 3 && head.cp < 0x800) return false;
	if (head.len == 4 && head.cp < 0x10000) return false;
	if (head.cp >= 0xd800 && head.cp <= 0xdfff) return false;
	return true;
}

static uint32_t tb_utf8_fallback(uint8_t c0, uint32_t *cp_out) { return tb_utf8_accept(cp_out, c0, 1); }

static uint32_t tb_utf8_char_len_bounded(char const *s, uint32_t len, uint32_t pos, uint32_t *cp_out) {
	if (!s || pos >= len) return 0;

	uint8_t    c0   = ( uint8_t ) s [pos];
	TbUtf8Head head = tb_utf8_head(c0);
	if (head.len == 1) return tb_utf8_accept(cp_out, head.cp, head.len);
	if (pos + head.len > len) return tb_utf8_fallback(c0, cp_out);
	if (!tb_utf8_read_tail(s, pos, &head)) return tb_utf8_fallback(c0, cp_out);
	if (!tb_utf8_scalar_valid(head)) return tb_utf8_fallback(c0, cp_out);
	return tb_utf8_accept(cp_out, head.cp, head.len);
}

static bool tb_utf8_is_cluster_extend(uint32_t cp) {
	return (cp >= 0x0300 && cp <= 0x036f)
	    || (cp >= 0x1ab0 && cp <= 0x1aff)
	    || (cp >= 0x1dc0 && cp <= 0x1dff)
	    || (cp >= 0x20d0 && cp <= 0x20ff)
	    || (cp >= 0xfe00 && cp <= 0xfe0f)
	    || (cp >= 0x1f3fb && cp <= 0x1f3ff);
}

bool tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed) {
	if (!tb) return false;
	if (needed <= tb->buf_cap) return true;

	uint32_t cap = tb->buf_cap ? tb->buf_cap : FLUX_TEXTBOX_INITIAL_CAP;
	while (cap < needed) {
		if (cap > UINT32_MAX / 2) {
			cap = needed;
			break;
		}
		cap = cap * 2;
	}

	char *nb = ( char * ) realloc(tb->buffer, cap);
	if (!nb) return false;
	tb->buffer       = nb;
	tb->buf_cap      = cap;
	tb->base.content = tb->buffer;
	return true;
}

uint32_t tb_utf8_char_len(char const *s, uint32_t len, uint32_t pos) {
	return tb_utf8_char_len_bounded(s, len, pos, NULL);
}

uint32_t tb_utf8_prev(char const *s, uint32_t len, uint32_t pos) {
	if (!s || pos == 0) return 0;
	if (pos > len) pos = len;

	uint32_t prev = 0;
	uint32_t cur  = 0;
	while (cur < pos) {
		uint32_t next = tb_utf8_next(s, len, cur);
		if (next == 0 || next > pos || next <= cur) break;
		prev = cur;
		cur  = next;
	}
	return prev;
}

uint32_t tb_utf8_next(char const *s, uint32_t len, uint32_t pos) {
	if (!s) return 0;
	if (pos >= len) return len;

	uint32_t cp       = 0;
	uint32_t clen     = tb_utf8_char_len_bounded(s, len, pos, &cp);
	uint32_t next     = pos + (clen ? clen : 1);
	uint32_t previous = cp;

	while (next < len) {
		uint32_t ext_cp  = 0;
		uint32_t ext_len = tb_utf8_char_len_bounded(s, len, next, &ext_cp);
		if (ext_len == 0) break;
		if (tb_utf8_is_cluster_extend(ext_cp) || ext_cp == 0x200d || previous == 0x200d) {
			next     += ext_len;
			previous  = ext_cp;
			continue;
		}
		break;
	}
	return next;
}

uint32_t tb_utf16_to_byte_offset(char const *s, uint32_t len, uint32_t utf16_pos) {
	uint32_t byte_off  = 0;
	uint32_t u16_count = 0;
	while (byte_off < len && u16_count < utf16_pos) {
		uint32_t clen  = tb_utf8_char_len(s, len, byte_off);
		u16_count     += (clen == 4) ? 2 : 1;
		byte_off      += clen;
	}
	return byte_off;
}

uint32_t tb_byte_to_utf16_offset(char const *s, uint32_t len, uint32_t byte_pos) {
	uint32_t byte_off  = 0;
	uint32_t u16_count = 0;
	while (byte_off < len && byte_off < byte_pos) {
		uint32_t clen  = tb_utf8_char_len(s, len, byte_off);
		u16_count     += (clen == 4) ? 2 : 1;
		byte_off      += clen;
	}
	return u16_count;
}

void tb_delete_range(FluxTextBoxInputData *tb, uint32_t from, uint32_t to) {
	if (from >= to || to > tb->buf_len) return;
	memmove(tb->buffer + from, tb->buffer + to, tb->buf_len - to);
	tb->buf_len              -= (to - from);
	tb->buffer [tb->buf_len]  = '\0';
}

void tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, char const *s, uint32_t slen) {
	if (!tb || !s || pos > tb->buf_len) return;
	if (slen > UINT32_MAX - tb->buf_len - 1) return;
	if (!tb_ensure_cap(tb, tb->buf_len + slen + 1)) return;
	memmove(tb->buffer + pos + slen, tb->buffer + pos, tb->buf_len - pos);
	memcpy(tb->buffer + pos, s, slen);
	tb->buf_len              += slen;
	tb->buffer [tb->buf_len]  = '\0';
}

bool tb_replace_text(FluxTextBoxInputData *tb, char const *text) {
	if (!tb) return false;
	if (!text) text = "";

	uint32_t len = ( uint32_t ) strlen(text);
	if (!tb_ensure_cap(tb, len + 1)) return false;

	memcpy(tb->buffer, text, ( size_t ) len + 1);
	tb->buf_len              = len;
	tb->base.content         = tb->buffer;
	tb->base.cursor_position = len;
	tb->base.selection_start = len;
	tb->base.selection_end   = len;
	return true;
}

void tb_delete_selection(FluxTextBoxInputData *tb) {
	uint32_t s = tb->base.selection_start < tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	uint32_t e = tb->base.selection_start > tb->base.selection_end ? tb->base.selection_start : tb->base.selection_end;
	if (s == e) return;
	tb_delete_range(tb, s, e);
	tb->base.cursor_position = s;
	tb->base.selection_start = s;
	tb->base.selection_end   = s;
}

bool     tb_has_selection(FluxTextBoxInputData const *tb) { return tb->base.selection_start != tb->base.selection_end; }

uint32_t tb_word_start(char const *s, uint32_t len, uint32_t pos) {
	if (pos == 0) return 0;
	uint32_t p = tb_utf8_prev(s, len, pos);
	while (p > 0 && (s [p] == ' ' || s [p] == '\t')) p = tb_utf8_prev(s, len, p);
	while (p > 0) {
		uint32_t prev = tb_utf8_prev(s, len, p);
		if (s [prev] == ' ' || s [prev] == '\t') break;
		p = prev;
	}
	return p;
}

uint32_t tb_word_end(char const *s, uint32_t len, uint32_t pos) {
	if (pos >= len) return len;
	uint32_t p = pos;
	while (p < len && s [p] != ' ' && s [p] != '\t') p = tb_utf8_next(s, len, p);
	while (p < len && (s [p] == ' ' || s [p] == '\t')) p = tb_utf8_next(s, len, p);
	return p;
}
