#include "tb_internal.h"

static bool pb_write_bullet(char *dst, uint32_t dst_cap, uint32_t *out) {
	if (*out + 3 >= dst_cap) return false;
	dst [(*out)++] = ( char ) 0xe2;
	dst [(*out)++] = ( char ) 0x97;
	dst [(*out)++] = ( char ) 0x8f;
	return true;
}

uint32_t pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap) {
	uint32_t out = 0;
	uint32_t pos = 0;
	while (pos < src_len && src [pos] && pb_write_bullet(dst, dst_cap, &out)) {
		uint32_t next = tb_utf8_next(src, src_len, pos);
		if (next <= pos || next > src_len) next = pos + 1;
		pos = next;
	}
	if (out < dst_cap) dst [out] = '\0';
	else if (dst_cap > 0) dst [dst_cap - 1] = '\0';
	return out;
}

uint32_t pb_mask_offset_to_original(char const *original, uint32_t original_len, uint32_t mask_byte_offset) {
	uint32_t target_cp = mask_byte_offset / 3;
	uint32_t cp        = 0;
	uint32_t pos       = 0;
	while (pos < original_len && original [pos] && cp++ < target_cp) {
		uint32_t next = tb_utf8_next(original, original_len, pos);
		if (next <= pos) next = pos + 1;
		pos = next;
	}
	return pos;
}

uint32_t pb_original_offset_to_mask(char const *original, uint32_t original_len, uint32_t original_byte_offset) {
	uint32_t cp  = 0;
	uint32_t pos = 0;
	uint32_t end = original_byte_offset < original_len ? original_byte_offset : original_len;
	while (pos < end && original [pos]) {
		uint32_t next = tb_utf8_next(original, original_len, pos);
		if (next <= pos || next > original_byte_offset) next = pos + 1;
		pos = next;
		cp++;
	}
	return cp * 3;
}
