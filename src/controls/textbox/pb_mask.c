/**
 * @file pb_mask.c
 * @brief PasswordBox mask text generation and offset conversion.
 */
#include "tb_internal.h"

/* Build mask string: each codepoint → ● (U+25CF = 3 UTF-8 bytes: E2 97 8F) */
uint32_t pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap) {
	uint32_t             out = 0;
	unsigned char const *p   = ( unsigned char const * ) src;
	unsigned char const *end = p + src_len;
		while (p < end && *p) {
			int skip;
			if (*p < 0x80) skip = 1;
			else if (*p < 0xe0) skip = 2;
			else if (*p < 0xf0) skip = 3;
			else skip = 4;
				if (out + 3 < dst_cap) {
					dst [out++] = ( char ) 0xe2;
					dst [out++] = ( char ) 0x97;
					dst [out++] = ( char ) 0x8f;
				}
			for (int i = 0; i < skip && p < end; i++) p++;
		}
	if (out < dst_cap) dst [out] = '\0';
	else if (dst_cap > 0) dst [dst_cap - 1] = '\0';
	return out;
}

/* Convert a byte offset in mask text back to byte offset in original text.
   Mask: each codepoint is 3 bytes. Original: variable-length. */
uint32_t pb_mask_offset_to_original(char const *original, uint32_t mask_byte_offset) {
	/* mask_byte_offset / 3 = codepoint index. Walk original to find that codepoint's byte offset. */
	uint32_t             target_cp = mask_byte_offset / 3;
	uint32_t             cp        = 0;
	unsigned char const *p         = ( unsigned char const * ) original;
		while (*p && cp < target_cp) {
			int skip;
			if (*p < 0x80) skip = 1;
			else if (*p < 0xe0) skip = 2;
			else if (*p < 0xf0) skip = 3;
			else skip = 4;
			for (int i = 0; i < skip && *p; i++) p++;
			cp++;
		}
	return ( uint32_t ) (p - ( unsigned char const * ) original);
}
