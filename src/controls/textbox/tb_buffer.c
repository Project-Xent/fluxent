#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

#define TB_ZWJ    0x200du
#define TB_KEYCAP 0x20e3u
#define TB_RI_LO  0x1f1e6u
#define TB_RI_HI  0x1f1ffu
#define TB_MOD_LO 0x1f3fbu
#define TB_MOD_HI 0x1f3ffu
#define TB_TAG_LO 0xe0020u
#define TB_TAG_HI 0xe007fu

typedef struct TbCpRange {
	uint32_t lo;
	uint32_t hi;
} TbCpRange;

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

static TbCpRange const tb_extend_ranges [] = {
  {0x0300,  0x036f },
  {0x0483,  0x0489 },
  {0x0591,  0x05bd },
  {0x05bf,  0x05bf },
  {0x05c1,  0x05c2 },
  {0x05c4,  0x05c5 },
  {0x05c7,  0x05c7 },
  {0x0610,  0x061a },
  {0x064b,  0x065f },
  {0x0670,  0x0670 },
  {0x06d6,  0x06dc },
  {0x06df,  0x06e4 },
  {0x06e7,  0x06e8 },
  {0x06ea,  0x06ed },
  {0x0711,  0x0711 },
  {0x0730,  0x074a },
  {0x1ab0,  0x1aff },
  {0x1dc0,  0x1dff },
  {0x20d0,  0x20ff },
  {0x2cef,  0x2cf1 },
  {0x2d7f,  0x2d7f },
  {0x2de0,  0x2dff },
  {0x302a,  0x302f },
  {0x3099,  0x309a },
  {0xa66f,  0xa672 },
  {0xa674,  0xa67d },
  {0xa69e,  0xa69f },
  {0xa6f0,  0xa6f1 },
  {0xa802,  0xa802 },
  {0xa806,  0xa806 },
  {0xa80b,  0xa80b },
  {0xa823,  0xa827 },
  {0xfb1e,  0xfb1e },
  {0xfe00,  0xfe0f },
  {0xfe20,  0xfe2f },
  {0x101fd, 0x101fd},
  {0x102e0, 0x102e0},
  {0x1d165, 0x1d169},
  {0x1d16d, 0x1d172},
  {0x1d17b, 0x1d182},
  {0x1d185, 0x1d18b},
  {0x1d1aa, 0x1d1ad},
  {0x1d242, 0x1d244},
  {0xe0100, 0xe01ef},
};

static TbCpRange const tb_emoji_ranges [] = {
  {0x2600,  0x27bf },
  {0x1f300, 0x1f6ff},
  {0x1f900, 0x1f9ff},
  {0x1fa70, 0x1faff},
};

static bool tb_in_range_table(TbCpRange const *table, size_t count, uint32_t cp) {
	size_t lo = 0;
	size_t hi = count;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (cp < table [mid].lo) hi = mid;
		else if (cp > table [mid].hi) lo = mid + 1;
		else return true;
	}
	return false;
}

static bool tb_cp_is_extend(uint32_t cp) {
	return tb_in_range_table(tb_extend_ranges, sizeof(tb_extend_ranges) / sizeof(tb_extend_ranges [0]), cp);
}

static bool tb_cp_is_emoji_like(uint32_t cp) {
	return tb_in_range_table(tb_emoji_ranges, sizeof(tb_emoji_ranges) / sizeof(tb_emoji_ranges [0]), cp);
}

static bool tb_cp_is_regional(uint32_t cp) { return cp >= TB_RI_LO && cp <= TB_RI_HI; }

static bool tb_cp_is_modifier(uint32_t cp) { return cp >= TB_MOD_LO && cp <= TB_MOD_HI; }

static bool tb_cp_is_tag(uint32_t cp) { return cp >= TB_TAG_LO && cp <= TB_TAG_HI; }

static bool tb_cp_is_emoji_or_keycap(uint32_t cp) {
	if (tb_cp_is_emoji_like(cp)) return true;
	if (tb_cp_is_regional(cp)) return true;
	return cp == TB_KEYCAP;
}

/** @brief Returns true if @p cp extends the cluster started by previous codepoint @p prev. */
static bool tb_extends_cluster(uint32_t prev, uint32_t cp) {
	if (tb_cp_is_extend(cp)) return true;
	if (cp == TB_ZWJ) return true;
	if (tb_cp_is_tag(cp)) return true;
	if (tb_cp_is_modifier(cp)) return tb_cp_is_emoji_like(prev) || prev == TB_ZWJ || tb_cp_is_modifier(prev);
	if (prev == TB_ZWJ && tb_cp_is_emoji_or_keycap(cp)) return true;
	if (tb_cp_is_regional(prev) && tb_cp_is_regional(cp)) return true;
	return false;
}

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
	tb->gap_end      = cap - suffix;
	tb->buffer       = nb;
	tb->buf_cap      = cap;
	tb->base.content = tb->buffer;
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
		if (!tb_extends_cluster(previous, ext_cp)) break;
		next     += ext_len;
		previous  = ext_cp;
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
	if (!tb) return;
	if (from >= to || to > tb->buf_len) return;

	tb_move_gap_to(tb, from);
	tb->gap_end += (to - from);
	tb->buf_len  = tb->gap_start + (tb->buf_cap - tb->gap_end);
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
