#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct TbUtf8Head {
	uint32_t cp;
	uint32_t len;
} TbUtf8Head;

typedef enum TbWordClass
{
	TB_WC_WORD  = 0,
	TB_WC_SPACE = 1,
	TB_WC_PUNCT = 2,
} TbWordClass;

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

static TbWordClass tb_word_class(uint8_t c) {
	if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x0b || c == 0x0c) return TB_WC_SPACE;
	if (c >= 'a' && c <= 'z') return TB_WC_WORD;
	if (c >= 'A' && c <= 'Z') return TB_WC_WORD;
	if (c >= '0' && c <= '9') return TB_WC_WORD;
	if (c == '_') return TB_WC_WORD;
	if (c >= 0x80) return TB_WC_WORD;
	return TB_WC_PUNCT;
}

static uint32_t tb_gap_size(FluxTextBoxInputData const *tb) {
	return tb->gap_end > tb->gap_start ? tb->gap_end - tb->gap_start : 0;
}

static uint32_t tb_physical_pos(FluxTextBoxInputData const *tb, uint32_t pos) {
	return pos < tb->gap_start ? pos : pos + tb_gap_size(tb);
}

static uint8_t tb_buffer_byte_at(FluxTextBoxInputData const *tb, uint32_t pos) {
	return ( uint8_t ) tb->buffer [tb_physical_pos(tb, pos)];
}

static uint32_t tb_buffer_utf8_char_len_bounded(FluxTextBoxInputData const *tb, uint32_t pos, uint32_t *cp_out) {
	if (!tb || !tb->buffer || pos >= tb->buf_len) return 0;

	uint8_t    c0   = tb_buffer_byte_at(tb, pos);
	TbUtf8Head head = tb_utf8_head(c0);
	if (head.len == 1) return tb_utf8_accept(cp_out, head.cp, head.len);
	if (pos + head.len > tb->buf_len) return tb_utf8_fallback(c0, cp_out);

	for (uint32_t i = 1; i < head.len; i++) {
		uint8_t cx = tb_buffer_byte_at(tb, pos + i);
		if (!tb_utf8_is_cont(cx)) return tb_utf8_fallback(c0, cp_out);
		head.cp = (head.cp << 6) | (cx & 0x3f);
	}
	if (!tb_utf8_scalar_valid(head)) return tb_utf8_fallback(c0, cp_out);
	return tb_utf8_accept(cp_out, head.cp, head.len);
}

static uint32_t tb_buffer_utf8_scalar_prev(FluxTextBoxInputData const *tb, uint32_t pos) {
	if (!tb || !tb->buffer || pos == 0) return 0;
	if (pos > tb->buf_len) pos = tb->buf_len;

	uint32_t p = pos - 1;
	while (p > 0 && tb_utf8_is_cont(tb_buffer_byte_at(tb, p))) p--;
	return p;
}

static void tb_move_gap_to(FluxTextBoxInputData *tb, uint32_t pos) {
	if (pos == tb->gap_start) return;
	if (pos < tb->gap_start) {
		uint32_t move_len = tb->gap_start - pos;
		memmove(tb->buffer + tb->gap_end - move_len, tb->buffer + pos, move_len);
		tb->gap_start -= move_len;
		tb->gap_end   -= move_len;
		return;
	}

	uint32_t move_len = pos - tb->gap_start;
	memmove(tb->buffer + tb->gap_start, tb->buffer + tb->gap_end, move_len);
	tb->gap_start += move_len;
	tb->gap_end   += move_len;
}

static void tb_mark_content_dirty(FluxTextBoxInputData *tb) {
	tb->flat_dirty   = true;
	tb->base.content = tb->flat_buffer ? tb->flat_buffer : "";
}

bool tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed) {
	if (!tb) return false;
	if (needed <= tb->buf_cap) return true;

	uint32_t cap = tb->buf_cap ? tb->buf_cap : FLUX_TEXTBOX_INITIAL_CAP;
	while (cap < needed) {
		if (cap > UINT32_MAX / TB_BUFFER_GROWTH_FACTOR) {
			cap = needed;
			break;
		}
		cap = cap * TB_BUFFER_GROWTH_FACTOR;
	}

	char *nb = ( char * ) realloc(tb->buffer, cap);
	if (!nb) return false;

	uint32_t suffix = tb->buf_cap - tb->gap_end;
	if (suffix > 0) memmove(nb + cap - suffix, nb + tb->gap_end, suffix);
	tb->gap_end = cap - suffix;
	tb->buffer  = nb;
	tb->buf_cap = cap;
	return true;
}

void tb_realize(FluxTextBoxInputData *tb) {
	if (!tb || !tb->buffer) return;

	uint32_t suffix = tb->buf_cap - tb->gap_end;
	if (suffix > 0) {
		memmove(tb->buffer + tb->gap_start, tb->buffer + tb->gap_end, suffix);
		tb->gap_start += suffix;
		tb->gap_end    = tb->buf_cap;
	}
	tb->buf_len              = tb->gap_start;
	tb->buffer [tb->buf_len] = '\0';
	tb->base.content         = tb->buffer;
	tb->flat_dirty           = true;
}

char const *tb_sync_content(FluxTextBoxInputData *tb) {
	if (!tb) return "";
	if (!tb->flat_dirty && tb->flat_buffer) return tb->flat_buffer;

	uint32_t needed = tb->buf_len + 1;
	if (needed > tb->flat_cap) {
		char *next = ( char * ) realloc(tb->flat_buffer, needed);
		if (!next) return tb->flat_buffer ? tb->flat_buffer : "";
		tb->flat_buffer = next;
		tb->flat_cap    = needed;
	}

	if (tb->gap_start > 0) memcpy(tb->flat_buffer, tb->buffer, tb->gap_start);
	uint32_t suffix = tb->buf_cap - tb->gap_end;
	if (suffix > 0) memcpy(tb->flat_buffer + tb->gap_start, tb->buffer + tb->gap_end, suffix);
	tb->flat_buffer [tb->buf_len] = '\0';
	tb->flat_dirty                = false;
	tb->base.content              = tb->flat_buffer;
	return tb->flat_buffer;
}

uint32_t tb_utf8_char_len(char const *s, uint32_t len, uint32_t pos) {
	return tb_utf8_char_len_bounded(s, len, pos, NULL);
}

uint32_t tb_utf8_prev(char const *s, uint32_t len, uint32_t pos) {
	if (!s || pos == 0) return 0;
	if (pos > len) pos = len;

	uint32_t cur = pos - 1;
	while (cur > 0 && tb_utf8_is_cont(( uint8_t ) s [cur])) cur--;
	return cur;
}

uint32_t tb_utf8_next(char const *s, uint32_t len, uint32_t pos) {
	if (!s) return 0;
	if (pos >= len) return len;

	uint32_t clen = tb_utf8_char_len_bounded(s, len, pos, NULL);
	return pos + (clen ? clen : 1);
}

uint32_t tb_buffer_utf8_prev(FluxTextBoxInputData const *tb, uint32_t pos) {
	if (!tb || !tb->buffer || pos == 0) return 0;
	return tb_buffer_utf8_scalar_prev(tb, pos);
}

uint32_t tb_buffer_utf8_next(FluxTextBoxInputData const *tb, uint32_t pos) {
	if (!tb || !tb->buffer) return 0;
	if (pos >= tb->buf_len) return tb->buf_len;

	uint32_t clen = tb_buffer_utf8_char_len_bounded(tb, pos, NULL);
	return pos + (clen ? clen : 1);
}

static FluxTextClusterQuery tb_cluster_query(FluxTextBoxInputData *tb, FluxTextRenderer *tr, FluxTextStyle *style) {
	char const *s              = tb_sync_content(tb);
	*style                     = tb_make_style(tb);
	FluxTextLayoutQuery layout = {tr, s, style, 0.0f};
	return (FluxTextClusterQuery) {layout, 0};
}

uint32_t tb_grapheme_next(FluxTextBoxInputData *tb, uint32_t pos) {
	if (!tb || !tb->buffer) return 0;
	if (pos >= tb->buf_len) return tb->buf_len;

	FluxTextRenderer *tr = flux_app_get_text_renderer(tb->app);
	if (!tr) return tb_buffer_utf8_next(tb, pos);

	FluxTextStyle        style;
	FluxTextClusterQuery q = tb_cluster_query(tb, tr, &style);
	q.index                = tb_byte_to_utf16_offset(q.layout.text, tb->buf_len, pos);
	return tb_utf16_to_byte_offset(q.layout.text, tb->buf_len, flux_text_cluster_next(&q));
}

uint32_t tb_grapheme_prev(FluxTextBoxInputData *tb, uint32_t pos) {
	if (!tb || !tb->buffer || pos == 0) return 0;

	FluxTextRenderer *tr = flux_app_get_text_renderer(tb->app);
	if (!tr) return tb_buffer_utf8_prev(tb, pos);

	FluxTextStyle        style;
	FluxTextClusterQuery q = tb_cluster_query(tb, tr, &style);
	q.index                = tb_byte_to_utf16_offset(q.layout.text, tb->buf_len, pos);
	return tb_utf16_to_byte_offset(q.layout.text, tb->buf_len, flux_text_cluster_prev(&q));
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
	if (!tb) return;
	if (from >= to || to > tb->buf_len) return;

	tb_move_gap_to(tb, from);
	tb->gap_end += (to - from);
	tb->buf_len  = tb->gap_start + (tb->buf_cap - tb->gap_end);
	tb_mark_content_dirty(tb);
}

void tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, char const *s, uint32_t slen) {
	if (!tb || !s || pos > tb->buf_len) return;
	if (slen == 0) return;
	if (slen > UINT32_MAX - tb->buf_len - 1) return;
	if (!tb_ensure_cap(tb, tb->buf_len + slen + 1)) return;

	tb_move_gap_to(tb, pos);
	memcpy(tb->buffer + tb->gap_start, s, slen);
	tb->gap_start += slen;
	tb->buf_len    = tb->gap_start + (tb->buf_cap - tb->gap_end);
	tb_mark_content_dirty(tb);
}

bool tb_replace_text(FluxTextBoxInputData *tb, char const *text) {
	if (!tb) return false;
	if (!text) text = "";

	uint32_t len = ( uint32_t ) strlen(text);
	if (!tb_ensure_cap(tb, len + 1)) return false;

	memcpy(tb->buffer, text, ( size_t ) len);
	tb->gap_start            = len;
	tb->gap_end              = tb->buf_cap;
	tb->buffer [len]         = '\0';
	tb->buf_len              = len;
	tb->base.cursor_position = len;
	tb->base.selection_start = len;
	tb->base.selection_end   = len;
	tb_mark_content_dirty(tb);
	( void ) tb_sync_content(tb);
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

/** @brief Word-boundary semantics follow Windows/WinUI: cursor stops at every class transition. */
uint32_t tb_word_start(char const *s, uint32_t len, uint32_t pos) {
	if (!s || pos == 0) return 0;
	if (pos > len) pos = len;

	uint32_t p = pos;
	while (p > 0) {
		uint32_t pp = tb_utf8_prev(s, len, p);
		if (tb_word_class(( uint8_t ) s [pp]) != TB_WC_SPACE) break;
		p = pp;
	}
	if (p == 0) return 0;

	uint32_t    pp_first = tb_utf8_prev(s, len, p);
	TbWordClass target   = tb_word_class(( uint8_t ) s [pp_first]);
	while (p > 0) {
		uint32_t pp = tb_utf8_prev(s, len, p);
		if (tb_word_class(( uint8_t ) s [pp]) != target) break;
		p = pp;
	}
	return p;
}

/** @brief Word-boundary semantics follow Windows/WinUI: cursor stops at every class transition. */
uint32_t tb_word_end(char const *s, uint32_t len, uint32_t pos) {
	if (!s || pos >= len) return len;

	TbWordClass target = tb_word_class(( uint8_t ) s [pos]);
	uint32_t    p      = pos;
	while (p < len && tb_word_class(( uint8_t ) s [p]) == target) p = tb_utf8_next(s, len, p);
	if (target == TB_WC_SPACE) return p;
	while (p < len && tb_word_class(( uint8_t ) s [p]) == TB_WC_SPACE) p = tb_utf8_next(s, len, p);
	return p;
}

uint32_t tb_buffer_word_start(FluxTextBoxInputData const *tb, uint32_t pos) {
	if (!tb || !tb->buffer || pos == 0) return 0;
	if (pos > tb->buf_len) pos = tb->buf_len;

	uint32_t p = pos;
	while (p > 0) {
		uint32_t pp = tb_buffer_utf8_prev(tb, p);
		if (tb_word_class(tb_buffer_byte_at(tb, pp)) != TB_WC_SPACE) break;
		p = pp;
	}
	if (p == 0) return 0;

	uint32_t    pp_first = tb_buffer_utf8_prev(tb, p);
	TbWordClass target   = tb_word_class(tb_buffer_byte_at(tb, pp_first));
	while (p > 0) {
		uint32_t pp = tb_buffer_utf8_prev(tb, p);
		if (tb_word_class(tb_buffer_byte_at(tb, pp)) != target) break;
		p = pp;
	}
	return p;
}

uint32_t tb_buffer_word_end(FluxTextBoxInputData const *tb, uint32_t pos) {
	if (!tb || !tb->buffer || pos >= tb->buf_len) return tb ? tb->buf_len : 0;

	TbWordClass target = tb_word_class(tb_buffer_byte_at(tb, pos));
	uint32_t    p      = pos;
	while (p < tb->buf_len && tb_word_class(tb_buffer_byte_at(tb, p)) == target) p = tb_buffer_utf8_next(tb, p);
	if (target == TB_WC_SPACE) return p;
	while (p < tb->buf_len && tb_word_class(tb_buffer_byte_at(tb, p)) == TB_WC_SPACE) p = tb_buffer_utf8_next(tb, p);
	return p;
}
