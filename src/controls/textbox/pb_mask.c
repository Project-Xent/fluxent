#include "tb_internal.h"

#define PB_MASK_BULLET_BYTES 3u

static bool pb_write_bullet(char *dst, uint32_t dst_cap, uint32_t *out) {
	if (*out + PB_MASK_BULLET_BYTES >= dst_cap) return false;
	dst [(*out)++] = ( char ) 0xe2;
	dst [(*out)++] = ( char ) 0x97;
	dst [(*out)++] = ( char ) 0x8f;
	return true;
}

static uint32_t pb_next_original_offset(char const *src, uint32_t src_len, uint32_t pos, uint32_t limit) {
	uint32_t next = tb_utf8_next(src, src_len, pos);
	return next > pos && next <= limit ? next : pos + 1;
}

uint32_t pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap) {
	uint32_t out = 0;
	uint32_t pos = 0;
	while (pos < src_len && src [pos] && pb_write_bullet(dst, dst_cap, &out))
		pos = pb_next_original_offset(src, src_len, pos, src_len);
	if (out < dst_cap) dst [out] = '\0';
	else if (dst_cap > 0) dst [dst_cap - 1] = '\0';
	return out;
}

uint32_t pb_mask_offset_to_original(char const *original, uint32_t original_len, uint32_t mask_byte_offset) {
	uint32_t target_cp = mask_byte_offset / PB_MASK_BULLET_BYTES;
	uint32_t cp        = 0;
	uint32_t pos       = 0;
	while (pos < original_len && original [pos] && cp++ < target_cp)
		pos = pb_next_original_offset(original, original_len, pos, original_len);
	return pos;
}

uint32_t pb_original_offset_to_mask(char const *original, uint32_t original_len, uint32_t original_byte_offset) {
	uint32_t cp  = 0;
	uint32_t pos = 0;
	uint32_t end = original_byte_offset < original_len ? original_byte_offset : original_len;
	while (pos < end && original [pos]) {
		pos = pb_next_original_offset(original, original_len, pos, end);
		cp++;
	}
	return cp * PB_MASK_BULLET_BYTES;
}
