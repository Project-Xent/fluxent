#include "text/flux_utf.h"

#include <stdlib.h>
#include <windows.h>

bool         flux_utf8_is_continuation(uint8_t byte) { return (byte & 0xc0) == 0x80; }

FluxUtf8Head flux_utf8_head(uint8_t lead) {
	if (lead < 0x80) return (FluxUtf8Head) {lead, 1};
	if (lead >= 0xc2 && lead <= 0xdf) return (FluxUtf8Head) {lead & 0x1fu, 2};
	if (lead >= 0xe0 && lead <= 0xef) return (FluxUtf8Head) {lead & 0x0fu, 3};
	if (lead >= 0xf0 && lead <= 0xf4) return (FluxUtf8Head) {lead & 0x07u, 4};
	return (FluxUtf8Head) {lead, 1};
}

bool flux_utf8_scalar_valid(FluxUtf8Head head) {
	if (head.len == 3 && head.cp < 0x800) return false;
	if (head.len == 4 && head.cp < 0x10000) return false;
	return head.cp < 0xd800 || head.cp > 0xdfff;
}

uint32_t flux_utf8_accept(uint32_t *out_cp, uint32_t cp, uint32_t len) {
	if (out_cp) *out_cp = cp;
	return len;
}

uint32_t    flux_utf8_fallback(uint8_t lead, uint32_t *out_cp) { return flux_utf8_accept(out_cp, lead, 1); }

static bool utf8_read_tail(char const *s, uint32_t pos, FluxUtf8Head *head) {
	for (uint32_t i = 1; i < head->len; i++) {
		uint8_t tail = ( uint8_t ) s [pos + i];
		if (!flux_utf8_is_continuation(tail)) return false;
		head->cp = (head->cp << 6) | (tail & 0x3fu);
	}
	return true;
}

uint32_t flux_utf8_decode(char const *s, uint32_t len, uint32_t pos, uint32_t *out_cp) {
	if (!s || pos >= len) return 0;

	uint8_t      lead = ( uint8_t ) s [pos];
	FluxUtf8Head head = flux_utf8_head(lead);
	if (head.len == 1) return flux_utf8_accept(out_cp, head.cp, head.len);
	if (pos + head.len > len) return flux_utf8_fallback(lead, out_cp);
	if (!utf8_read_tail(s, pos, &head)) return flux_utf8_fallback(lead, out_cp);
	if (!flux_utf8_scalar_valid(head)) return flux_utf8_fallback(lead, out_cp);
	return flux_utf8_accept(out_cp, head.cp, head.len);
}

uint32_t flux_utf8_char_len(char const *s, uint32_t len, uint32_t pos) { return flux_utf8_decode(s, len, pos, NULL); }

uint32_t flux_utf8_next(char const *s, uint32_t len, uint32_t pos) {
	if (!s) return 0;
	if (pos >= len) return len;

	uint32_t size = flux_utf8_char_len(s, len, pos);
	return pos + (size ? size : 1u);
}

static uint32_t utf16_units_for(uint32_t byte_len) { return byte_len == 4u ? 2u : 1u; }

uint32_t        flux_utf8_to_utf16_offset(char const *s, uint32_t len, uint32_t byte_pos) {
	uint32_t byte_at = 0u;
	uint32_t units   = 0u;
	while (byte_at < len && byte_at < byte_pos) {
		uint32_t size = flux_utf8_char_len(s, len, byte_at);
		if (!size) break;
		units   += utf16_units_for(size);
		byte_at += size;
	}
	return units;
}

uint32_t flux_utf16_to_utf8_offset(char const *s, uint32_t len, uint32_t utf16_pos) {
	uint32_t byte_at = 0u;
	uint32_t units   = 0u;
	while (byte_at < len && units < utf16_pos) {
		uint32_t size = flux_utf8_char_len(s, len, byte_at);
		if (!size) break;
		units   += utf16_units_for(size);
		byte_at += size;
	}
	return byte_at;
}

uint32_t *flux_utf8_utf16_byte_table(char const *s, uint32_t len, uint32_t utf16_len) {
	uint32_t *table = ( uint32_t * ) malloc(sizeof(*table) * ( size_t ) (utf16_len + 1u));
	if (!table) return NULL;

	uint32_t byte_at = 0u;
	uint32_t unit    = 0u;
	while (byte_at < len && unit <= utf16_len) {
		uint32_t size = flux_utf8_char_len(s, len, byte_at);
		if (!size) break;
		for (uint32_t i = 0u; i < utf16_units_for(size) && unit <= utf16_len; ++i) table [unit++] = byte_at;
		byte_at += size;
	}
	while (unit <= utf16_len) table [unit++] = byte_at;
	return table;
}

int flux_utf8_to_wchar(char const *utf8, wchar_t *out, int out_cap) {
	if (!utf8 || !utf8 [0]) {
		if (out && out_cap > 0) out [0] = L'\0';
		return 0;
	}
	int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (needed <= 0) return 0;
	if (!out) return needed;
	if (needed > out_cap) needed = out_cap;
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, needed);
	return needed;
}

bool flux_wide_buffer_from_utf8(FluxWideBuffer *buf, char const *utf8) {
	memset(buf, 0, sizeof(*buf));
	if (utf8 && utf8 [0] == '\0') {
		buf->text      = buf->stack;
		buf->stack [0] = L'\0';
		return true;
	}

	int needed = flux_utf8_to_wchar(utf8, NULL, 0);
	if (needed <= 0) return false;

	buf->text = (needed <= ( int ) (sizeof(buf->stack) / sizeof(buf->stack [0])))
	            ? buf->stack
	            : ( wchar_t * ) malloc(sizeof(wchar_t) * ( size_t ) needed);
	if (!buf->text) return false;

	flux_utf8_to_wchar(utf8, buf->text, needed);
	buf->len = needed - 1;
	return true;
}

void flux_wide_buffer_free(FluxWideBuffer *buf) {
	if (buf->text && buf->text != buf->stack) free(buf->text);
	buf->text = NULL;
	buf->len  = 0;
}
