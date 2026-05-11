#include "tb_internal.h"

#define PB_BULLET_UTF8_LEN 3u

static uint32_t pb_next_char(char const *src, uint32_t src_len, uint32_t pos) {
	uint32_t next = tb_utf8_next(src, src_len, pos);
	return (next <= pos || next > src_len) ? pos + 1 : next;
}

static uint32_t pb_count_chars_until(char const *src, uint32_t src_len, uint32_t byte_limit) {
	uint32_t cp  = 0;
	uint32_t pos = 0;
	uint32_t end = byte_limit < src_len ? byte_limit : src_len;
	while (pos < end && src [pos]) {
		pos = pb_next_char(src, src_len, pos);
		cp++;
	}
	return cp;
}

static bool pb_write_bullet(char *dst, uint32_t dst_cap, uint32_t *out) {
	if (*out + PB_BULLET_UTF8_LEN >= dst_cap) return false;
	dst [(*out)++] = ( char ) 0xe2;
	dst [(*out)++] = ( char ) 0x97;
	dst [(*out)++] = ( char ) 0x8f;
	return true;
}

uint32_t pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap) {
	uint32_t out = 0;
	uint32_t pos = 0;
	while (pos < src_len && src [pos] && pb_write_bullet(dst, dst_cap, &out)) pos = pb_next_char(src, src_len, pos);
	if (out < dst_cap) dst [out] = '\0';
	else if (dst_cap > 0) dst [dst_cap - 1] = '\0';
	return out;
}

uint32_t pb_mask_offset_to_original(char const *original, uint32_t original_len, uint32_t mask_byte_offset) {
	uint32_t target_cp = mask_byte_offset / PB_BULLET_UTF8_LEN;
	uint32_t cp        = 0;
	uint32_t pos       = 0;
	while (pos < original_len && original [pos] && cp++ < target_cp) pos = pb_next_char(original, original_len, pos);
	return pos;
}

uint32_t pb_original_offset_to_mask(char const *original, uint32_t original_len, uint32_t original_byte_offset) {
	return pb_count_chars_until(original, original_len, original_byte_offset) * PB_BULLET_UTF8_LEN;
}
