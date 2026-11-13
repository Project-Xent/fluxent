#ifndef FLUX_UTF_H
#define FLUX_UTF_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct FluxUtf8Head {
	uint32_t cp;
	uint32_t len;
} FluxUtf8Head;

bool         flux_utf8_is_continuation(uint8_t byte);
FluxUtf8Head flux_utf8_head(uint8_t lead);
bool         flux_utf8_scalar_valid(FluxUtf8Head head);
uint32_t     flux_utf8_accept(uint32_t *out_cp, uint32_t cp, uint32_t len);

uint32_t     flux_utf8_fallback(uint8_t lead, uint32_t *out_cp);

uint32_t     flux_utf8_decode(char const *s, uint32_t len, uint32_t pos, uint32_t *out_cp);
uint32_t     flux_utf8_char_len(char const *s, uint32_t len, uint32_t pos);
uint32_t     flux_utf8_next(char const *s, uint32_t len, uint32_t pos);

uint32_t     flux_utf8_to_utf16_offset(char const *s, uint32_t len, uint32_t byte_pos);
uint32_t     flux_utf16_to_utf8_offset(char const *s, uint32_t len, uint32_t utf16_pos);

uint32_t    *flux_utf8_utf16_byte_table(char const *s, uint32_t len, uint32_t utf16_len);

typedef struct FluxWideBuffer {
	wchar_t  stack [256];
	wchar_t *text;
	int      len;
} FluxWideBuffer;

int  flux_utf8_to_wchar(char const *utf8, wchar_t *out, int out_cap);
bool flux_wide_buffer_from_utf8(FluxWideBuffer *buf, char const *utf8);
void flux_wide_buffer_free(FluxWideBuffer *buf);

#endif
