/**
 * @file tb_buffer.c
 * @brief Buffer manipulation and UTF-8 helpers for TextBox.
 */
#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

void tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed) {
	if (needed <= tb->buf_cap) return;
	uint32_t cap = tb->buf_cap;
	while (cap < needed) cap = cap * 2;
	char *nb = ( char * ) realloc(tb->buffer, cap);
	if (!nb) return;
	tb->buffer       = nb;
	tb->buf_cap      = cap;
	tb->base.content = tb->buffer;
}

uint32_t tb_utf8_char_len(char const *s, uint32_t pos) {
	if (pos >= ( uint32_t ) strlen(s)) return 0;
	uint8_t c = ( uint8_t ) s [pos];
	if (c < 0x80) return 1;
	if ((c & 0xe0) == 0xc0) return 2;
	if ((c & 0xf0) == 0xe0) return 3;
	if ((c & 0xf8) == 0xf0) return 4;
	return 1;
}

uint32_t tb_utf8_prev(char const *s, uint32_t pos) {
	if (pos == 0) return 0;
	uint32_t p = pos - 1;
	while (p > 0 && (( uint8_t ) s [p] & 0xc0) == 0x80) p--;
	return p;
}

uint32_t tb_utf8_next(char const *s, uint32_t pos) {
	uint32_t len = ( uint32_t ) strlen(s);
	if (pos >= len) return len;
	return pos + tb_utf8_char_len(s, pos);
}

uint32_t tb_utf16_to_byte_offset(char const *s, uint32_t utf16_pos) {
	uint32_t byte_off  = 0;
	uint32_t u16_count = 0;
	uint32_t len       = ( uint32_t ) strlen(s);
		while (byte_off < len && u16_count < utf16_pos) {
			uint32_t clen  = tb_utf8_char_len(s, byte_off);
			u16_count     += (clen == 4) ? 2 : 1;
			byte_off      += clen;
		}
	return byte_off;
}

uint32_t tb_byte_to_utf16_offset(char const *s, uint32_t byte_pos) {
	uint32_t byte_off  = 0;
	uint32_t u16_count = 0;
	uint32_t len       = ( uint32_t ) strlen(s);
		while (byte_off < len && byte_off < byte_pos) {
			uint32_t clen  = tb_utf8_char_len(s, byte_off);
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
	tb_ensure_cap(tb, tb->buf_len + slen + 1);
	memmove(tb->buffer + pos + slen, tb->buffer + pos, tb->buf_len - pos);
	memcpy(tb->buffer + pos, s, slen);
	tb->buf_len              += slen;
	tb->buffer [tb->buf_len]  = '\0';
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

uint32_t tb_word_start(char const *s, uint32_t pos) {
	if (pos == 0) return 0;
	uint32_t p = tb_utf8_prev(s, pos);
	/* Skip spaces first */
	while (p > 0 && (s [p] == ' ' || s [p] == '\t')) p = tb_utf8_prev(s, p);
	/* Then skip non-spaces */
	while (p > 0 && s [tb_utf8_prev(s, p)] != ' ' && s [tb_utf8_prev(s, p)] != '\t') p = tb_utf8_prev(s, p);
	return p;
}

uint32_t tb_word_end(char const *s, uint32_t pos) {
	uint32_t len = ( uint32_t ) strlen(s);
	if (pos >= len) return len;
	uint32_t p = pos;
	/* Skip non-spaces first */
	while (p < len && s [p] != ' ' && s [p] != '\t') p = tb_utf8_next(s, p);
	/* Then skip spaces */
	while (p < len && (s [p] == ' ' || s [p] == '\t')) p = tb_utf8_next(s, p);
	return p;
}
