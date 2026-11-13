#include "text/flux_text_internal.h"

#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a_from(uint32_t seed, void const *data, size_t len) {
	uint8_t const *p = ( uint8_t const * ) data;
	for (size_t i = 0; i < len; i++) {
		seed ^= p [i];
		seed *= 0x01000193u;
	}
	return seed;
}

static uint32_t fnv1a_str_from(uint32_t seed, char const *s) {
	if (!s) return seed;
	return fnv1a_from(seed, s, strlen(s));
}

static uint32_t fnv1a_str(char const *s) { return fnv1a_str_from(0x811c9dc5u, s); }

static uint32_t fnv1a_bytes(void const *data, size_t len) { return fnv1a_from(0x811c9dc5u, data, len); }

static uint32_t float_to_bits(float f) {
	uint32_t bits;
	memcpy(&bits, &f, sizeof(bits));
	return bits;
}

static IDWriteTextFormat *create_styled_format(FluxText *tr, FluxTextStyle const *s) {
	wchar_t const *family = FLUX_DEFAULT_FONT_FAMILY;
	wchar_t        fam_buf [128];
	if (s->font_family && s->font_family [0]) {
		int n = flux_utf8_to_wchar(s->font_family, fam_buf, 128);
		if (n > 0) family = fam_buf;
	}
	float              size   = s->font_size > 0.0f ? s->font_size : 14.0f;
	DWRITE_FONT_WEIGHT weight = ( DWRITE_FONT_WEIGHT ) (s->font_weight > 0 ? s->font_weight : 400);

	IDWriteTextFormat *fmt    = NULL;
	HRESULT            hr     = IDWriteFactory_CreateTextFormat(
	  tr->factory, family, NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &fmt
	);
	if (FAILED(hr)) return NULL;

	IDWriteTextFormat_SetTextAlignment(fmt, to_dw_align(s->text_align));
	IDWriteTextFormat_SetParagraphAlignment(fmt, to_dw_para(s->vert_align));
	if (!s->word_wrap) IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP);

	return fmt;
}

static FluxFormatCacheKey make_format_key(FluxTextStyle const *s) {
	FluxFormatCacheKey k;
	k.family_hash = fnv1a_str(s->font_family);
	k.size_bits   = float_to_bits(s->font_size > 0.0f ? s->font_size : 14.0f);
	k.font_weight = s->font_weight > 0 ? s->font_weight : 400;
	k.text_align  = ( int ) s->text_align;
	k.vert_align  = ( int ) s->vert_align;
	k.word_wrap   = s->word_wrap ? 1 : 0;
	return k;
}

#define DEFINE_TEXT_CACHE_ACCESSORS(                                                                \
  Name, EntryType, KeyType, ValueType, cache_field, cache_size, value_field, release_fn             \
)                                                                                                   \
	static ValueType *find_cached_##Name(FluxText *tr, KeyType const *key) {                        \
		for (int i = 0; i < cache_size; i++) {                                                      \
			EntryType *e = &tr->cache_field [i];                                                    \
			if (!e->occupied || memcmp(&e->key, key, sizeof(*key)) != 0) continue;                  \
			e->last_used = tr->cache_tick;                                                          \
			return e->value_field;                                                                  \
		}                                                                                           \
		return NULL;                                                                                \
	}                                                                                               \
	static int find_##Name##_cache_slot(FluxText *tr) {                                             \
		int      slot   = 0;                                                                        \
		uint64_t oldest = UINT64_MAX;                                                               \
		for (int i = 0; i < cache_size; i++) {                                                      \
			if (!tr->cache_field [i].occupied) return i;                                            \
			if (tr->cache_field [i].last_used >= oldest) continue;                                  \
			oldest = tr->cache_field [i].last_used;                                                 \
			slot   = i;                                                                             \
		}                                                                                           \
		return slot;                                                                                \
	}                                                                                               \
	static void store_cached_##Name(FluxText *tr, int slot, KeyType const *key, ValueType *value) { \
		if (tr->cache_field [slot].occupied && tr->cache_field [slot].value_field)                  \
			release_fn(tr->cache_field [slot].value_field);                                         \
		tr->cache_field [slot].key         = *key;                                                  \
		tr->cache_field [slot].value_field = value;                                                 \
		tr->cache_field [slot].last_used   = tr->cache_tick;                                        \
		tr->cache_field [slot].occupied    = true;                                                  \
	}

DEFINE_TEXT_CACHE_ACCESSORS(
  format, FluxFormatCacheEntry, FluxFormatCacheKey, IDWriteTextFormat, format_cache, FLUX_FORMAT_CACHE_SIZE, format,
  IDWriteTextFormat_Release
)

static IDWriteTextFormat *get_or_create_format(FluxText *tr, FluxTextStyle const *s) {
	FluxFormatCacheKey key = make_format_key(s);
	IDWriteTextFormat *fmt = find_cached_format(tr, &key);
	if (fmt) return fmt;

	fmt = create_styled_format(tr, s);
	if (!fmt) return NULL;
	store_cached_format(tr, find_format_cache_slot(tr), &key, fmt);
	return fmt;
}

static uint32_t hash_style_extras(FluxTextStyle const *s) {
	uint32_t h = fnv1a_bytes(&s->run_count, sizeof(s->run_count));
	h          = fnv1a_from(h, &s->trim, sizeof(s->trim));
	h          = fnv1a_from(h, &s->max_lines, sizeof(s->max_lines));
	for (uint32_t i = 0u; i < s->run_count; ++i) {
		XentTextRun const *r = &s->runs [i];
		h                    = fnv1a_from(h, &r->offset, sizeof(r->offset));
		h                    = fnv1a_from(h, &r->length, sizeof(r->length));
		h                    = fnv1a_from(h, &r->font_size, sizeof(r->font_size));
		h                    = fnv1a_from(h, &r->font_weight, sizeof(r->font_weight));
		h                    = fnv1a_from(h, &r->font_stretch, sizeof(r->font_stretch));
		h                    = fnv1a_from(h, &r->slant, sizeof(r->slant));
		h                    = fnv1a_str_from(h, r->font_family);
	}
	return h;
}

static FluxLayoutCacheKey make_layout_key(char const *utf8_text, FluxTextStyle const *s, float max_w, float max_h) {
	FluxFormatCacheKey fk = make_format_key(s);
	FluxLayoutCacheKey key;
	key.text_hash   = fnv1a_str(utf8_text);
	key.text_len    = ( uint32_t ) (utf8_text ? strlen(utf8_text) : 0);
	key.format_hash = fnv1a_bytes(&fk, sizeof(fk));
	key.style_hash  = hash_style_extras(s);
	key.max_w_bits  = float_to_bits(max_w);
	key.max_h_bits  = float_to_bits(max_h);
	return key;
}

DEFINE_TEXT_CACHE_ACCESSORS(
  layout, FluxLayoutCacheEntry, FluxLayoutCacheKey, IDWriteTextLayout, layout_cache, FLUX_LAYOUT_CACHE_SIZE, layout,
  IDWriteTextLayout_Release
)

IDWriteTextLayout *get_or_create_layout(FluxText *tr, TextLayoutCacheRequest const *req) {
	FluxLayoutCacheKey key    = make_layout_key(req->utf8_text, req->style, req->max_w, req->max_h);
	IDWriteTextLayout *layout = find_cached_layout(tr, &key);
	if (layout) return layout;

	IDWriteTextFormat *fmt = get_or_create_format(tr, req->style);
	if (!fmt) return NULL;

	layout     = NULL;
	HRESULT hr = IDWriteFactory_CreateTextLayout(
	  tr->factory, req->wtext, ( UINT32 ) req->wlen, fmt, req->max_w, req->max_h, &layout
	);
	if (FAILED(hr)) return NULL;

	text_configure_layout(tr, layout, req->utf8_text, req->style);
	store_cached_layout(tr, find_layout_cache_slot(tr), &key, layout);
	return layout;
}
