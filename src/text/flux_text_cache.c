#include "text/flux_text_internal.h"

#include <stdlib.h>
#include <string.h>

static uint64_t fnv1a_from(uint64_t seed, void const *data, size_t len) {
	uint8_t const *p = ( uint8_t const * ) data;
	for (size_t i = 0; i < len; i++) {
		seed ^= p [i];
		seed *= UINT64_C(0x100000001b3);
	}
	return seed;
}

static uint64_t fnv1a_bytes(void const *data, size_t len) {
	return fnv1a_from(UINT64_C(0xcbf29ce484222325), data, len);
}

static uint64_t fnv1a_str_from(uint64_t seed, char const *s) {
	return s ? fnv1a_from(seed, s, strlen(s) + 1u) : fnv1a_from(seed, "", 1u);
}

static uint32_t float_to_bits(float f) {
	uint32_t bits;
	memcpy(&bits, &f, sizeof(bits));
	return bits;
}

static char const *fmt_family_ptr(FluxFormatCacheKey const *k) {
	return k->family_heap ? k->family_heap : k->family_inline;
}

static void fmt_key_free(FluxFormatCacheKey *k) {
	free(k->family_heap);
	k->family_heap = NULL;
}

static bool fmt_key_init_family(FluxFormatCacheKey *k, char const *fam, uint32_t len) {
	k->family_len  = len;
	k->family_heap = NULL;
	if (!fam || !len) {
		k->family_inline [0] = '\0';
		return true;
	}
	if (len < sizeof(k->family_inline)) {
		memcpy(k->family_inline, fam, len + 1u);
		return true;
	}
	k->family_heap = ( char * ) malloc(len + 1u);
	if (!k->family_heap) return false;
	memcpy(k->family_heap, fam, len + 1u);
	return true;
}

static void
make_format_spec(FluxTextStyle const *s, DWRITE_WORD_WRAPPING wrap, FluxFormatCacheKey *k, char const **out_fam) {
	char const *fam = (s->font_family && s->font_family [0]) ? s->font_family : "";
	uint32_t    len = ( uint32_t ) strlen(fam);
	memset(k, 0, sizeof(*k));
	k->family_len  = len;
	k->size_bits   = float_to_bits(s->font_size > 0.0f ? s->font_size : 14.0f);
	k->font_weight = s->font_weight > 0 ? s->font_weight : 400;
	k->text_align  = ( int ) s->text_align;
	k->vert_align  = ( int ) s->vert_align;
	k->wrapping    = wrap;
	k->rtl         = s->rtl ? 1 : 0;
	uint64_t h     = fnv1a_bytes(fam, len + 1u);
	h              = fnv1a_from(h, &k->size_bits, sizeof(k->size_bits));
	h              = fnv1a_from(h, &k->font_weight, sizeof(k->font_weight));
	h              = fnv1a_from(h, &k->text_align, sizeof(k->text_align));
	h              = fnv1a_from(h, &k->vert_align, sizeof(k->vert_align));
	h              = fnv1a_from(h, &k->wrapping, sizeof(k->wrapping));
	h              = fnv1a_from(h, &k->rtl, sizeof(k->rtl));
	k->hash        = h;
	*out_fam       = fam;
}

static bool fmt_key_equal(FluxFormatCacheKey const *a, FluxFormatCacheKey const *b, char const *b_fam) {
	if (a->hash != b->hash || a->family_len != b->family_len || a->size_bits != b->size_bits) return false;
	if (a->font_weight != b->font_weight || a->text_align != b->text_align || a->vert_align != b->vert_align)
		return false;
	if (a->wrapping != b->wrapping || a->rtl != b->rtl) return false;
	return memcmp(fmt_family_ptr(a), b_fam, a->family_len) == 0;
}

static FluxTextCaches *text_ensure_caches(FluxText *tr) {
	if (tr->caches) return tr->caches;
	FluxTextCaches *c = ( FluxTextCaches * ) calloc(1, sizeof(*c));
	if (!c) return NULL;
	memset(c->format_buckets, -1, sizeof(c->format_buckets));
	memset(c->layout_buckets, -1, sizeof(c->layout_buckets));
	c->fmt_head = c->fmt_tail = -1;
	c->lay_head = c->lay_tail = -1;
	tr->caches                = c;
	return c;
}

static void layout_key_free(FluxLayoutCacheKey *k) {
	free(k->text_heap);
	k->text_heap = NULL;
	fmt_key_free(&k->fmt_spec);
}

void flux_text_caches_release(FluxText *tr) {
	if (!tr || !tr->caches) return;
	FluxTextCaches *c = tr->caches;
	for (int i = 0; i < FLUX_FORMAT_CACHE_CAP; i++) {
		if (!c->format_pool [i].in_use) continue;
		if (c->format_pool [i].format) IDWriteTextFormat_Release(c->format_pool [i].format);
		fmt_key_free(&c->format_pool [i].key);
	}
	for (int i = 0; i < FLUX_LAYOUT_CACHE_CAP; i++) {
		if (!c->layout_pool [i].in_use) continue;
		if (c->layout_pool [i].layout) IDWriteTextLayout_Release(c->layout_pool [i].layout);
		layout_key_free(&c->layout_pool [i].key);
	}
	free(c);
	tr->caches = NULL;
}

static void lru_unlink_fmt(FluxTextCaches *c, int16_t idx) {
	FluxFormatCacheEntry *e = &c->format_pool [idx];
	if (e->lru_prev >= 0) c->format_pool [e->lru_prev].lru_next = e->lru_next;
	else c->fmt_head = e->lru_next;
	if (e->lru_next >= 0) c->format_pool [e->lru_next].lru_prev = e->lru_prev;
	else c->fmt_tail = e->lru_prev;
	e->lru_prev = e->lru_next = -1;
}

static void lru_push_front_fmt(FluxTextCaches *c, int16_t idx) {
	FluxFormatCacheEntry *e = &c->format_pool [idx];
	e->lru_prev             = -1;
	e->lru_next             = c->fmt_head;
	if (c->fmt_head >= 0) c->format_pool [c->fmt_head].lru_prev = idx;
	else c->fmt_tail = idx;
	c->fmt_head = idx;
}

static void bucket_unlink_fmt(FluxTextCaches *c, int16_t idx) {
	uint32_t b   = ( uint32_t ) c->format_pool [idx].key.hash & (FLUX_FORMAT_HASH_CAP - 1u);
	int16_t *cur = &c->format_buckets [b];
	while (*cur >= 0) {
		if (*cur == idx) {
			*cur                           = c->format_pool [idx].hash_next;
			c->format_pool [idx].hash_next = -1;
			return;
		}
		cur = &c->format_pool [*cur].hash_next;
	}
}

static int16_t alloc_fmt_slot(FluxTextCaches *c) {
	if (c->fmt_count < FLUX_FORMAT_CACHE_CAP) return ( int16_t ) c->fmt_count++;
	int16_t victim = c->fmt_tail;
	lru_unlink_fmt(c, victim);
	bucket_unlink_fmt(c, victim);
	FluxFormatCacheEntry *e = &c->format_pool [victim];
	if (e->format) IDWriteTextFormat_Release(e->format);
	fmt_key_free(&e->key);
	e->format = NULL;
	e->in_use = false;
	return victim;
}

static IDWriteTextFormat *create_styled_format(FluxText *tr, FluxTextStyle const *s, DWRITE_WORD_WRAPPING wrap) {
	wchar_t const *family = tr->default_font ? tr->default_font : FLUX_DEFAULT_FONT_FAMILY;
	wchar_t        fam_buf [FLUX_FAMILY_BUF];
	if (s->font_family && s->font_family [0] && flux_utf8_to_wchar(s->font_family, fam_buf, FLUX_FAMILY_BUF) > 0)
		family = fam_buf;
	float              size   = s->font_size > 0.0f ? s->font_size : 14.0f;
	DWRITE_FONT_WEIGHT weight = ( DWRITE_FONT_WEIGHT ) (s->font_weight > 0 ? s->font_weight : 400);

	IDWriteTextFormat *fmt    = NULL;
	HRESULT            hr     = IDWriteFactory_CreateTextFormat(
	  tr->factory, family, NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &fmt
	);
	if (FAILED(hr)) return NULL;

	IDWriteTextFormat_SetTextAlignment(fmt, to_dw_align(s->text_align));
	IDWriteTextFormat_SetParagraphAlignment(fmt, to_dw_para(s->vert_align));
	IDWriteTextFormat_SetWordWrapping(fmt, wrap);
	if (s->rtl) IDWriteTextFormat_SetReadingDirection(fmt, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
	return fmt;
}

IDWriteTextFormat *get_or_create_format_ex(FluxText *tr, FluxTextStyle const *s, DWRITE_WORD_WRAPPING wrap) {
	FluxFormatCacheKey spec;
	char const        *fam = NULL;
	make_format_spec(s, wrap, &spec, &fam);
	FluxTextCaches *c = text_ensure_caches(tr);
	uint32_t        b = ( uint32_t ) spec.hash & (FLUX_FORMAT_HASH_CAP - 1u);
	for (int16_t i = c ? c->format_buckets [b] : -1; i >= 0; i = c->format_pool [i].hash_next) {
		if (!fmt_key_equal(&c->format_pool [i].key, &spec, fam)) continue;
		lru_unlink_fmt(c, i);
		lru_push_front_fmt(c, i);
		return c->format_pool [i].format;
	}
	IDWriteTextFormat *fmt = create_styled_format(tr, s, wrap);
	if (fmt && (!c || !fmt_key_init_family(&spec, fam, spec.family_len))) {
		IDWriteTextFormat_Release(fmt);
		return NULL;
	}
	if (!fmt) return NULL;
	int16_t slot                    = alloc_fmt_slot(c);
	c->format_pool [slot].key       = spec;
	c->format_pool [slot].format    = fmt;
	c->format_pool [slot].in_use    = true;
	c->format_pool [slot].hash_next = c->format_buckets [b];
	c->format_buckets [b]           = slot;
	lru_push_front_fmt(c, slot);
	return fmt;
}

static uint64_t hash_style_extras(FluxTextStyle const *s) {
	uint64_t h = fnv1a_bytes(&s->run_count, sizeof(s->run_count));
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

static char const *layout_text_ptr(FluxLayoutCacheKey const *k) { return k->text_heap ? k->text_heap : k->text_inline; }

static bool        layout_key_init_text(FluxLayoutCacheKey *k, char const *txt, uint32_t len, char const *fam) {
	if (!fmt_key_init_family(&k->fmt_spec, fam, k->fmt_spec.family_len)) return false;
	k->text_heap = NULL;
	if (len < sizeof(k->text_inline)) {
		memcpy(k->text_inline, txt, len);
		k->text_inline [len] = '\0';
		return true;
	}
	k->text_heap = ( char * ) malloc(len + 1u);
	if (!k->text_heap) {
		fmt_key_free(&k->fmt_spec);
		return false;
	}
	memcpy(k->text_heap, txt, len);
	k->text_heap [len] = '\0';
	return true;
}

static void make_layout_spec(
  TextLayoutRequest const *req, DWRITE_WORD_WRAPPING wrap, FluxLayoutCacheKey *k, char const **out_txt,
  char const **out_fam
) {
	char const *txt = req->text ? req->text : "";
	uint32_t    len = ( uint32_t ) strlen(txt);
	memset(k, 0, sizeof(*k));
	make_format_spec(req->style, wrap, &k->fmt_spec, out_fam);
	k->text_len    = len;
	k->format_hash = k->fmt_spec.hash;
	k->style_hash  = hash_style_extras(req->style);
	k->run_count   = req->style->run_count;
	k->trim        = req->style->trim;
	k->max_lines   = req->style->max_lines;
	k->max_w_bits  = float_to_bits(req->max_w);
	k->max_h_bits  = float_to_bits(req->max_h);
	uint64_t h     = fnv1a_bytes(txt, len + 1u);
	h              = fnv1a_from(h, &k->format_hash, sizeof(k->format_hash));
	h              = fnv1a_from(h, &k->style_hash, sizeof(k->style_hash));
	h              = fnv1a_from(h, &k->max_w_bits, sizeof(k->max_w_bits));
	h              = fnv1a_from(h, &k->max_h_bits, sizeof(k->max_h_bits));
	k->hash        = h;
	*out_txt       = txt;
}

static bool
layout_key_equal(FluxLayoutCacheKey const *a, FluxLayoutCacheKey const *b, char const *b_txt, char const *b_fam) {
	if (a->hash != b->hash || a->text_len != b->text_len || a->style_hash != b->style_hash) return false;
	if (a->run_count != b->run_count || a->trim != b->trim || a->max_lines != b->max_lines) return false;
	if (a->max_w_bits != b->max_w_bits || a->max_h_bits != b->max_h_bits) return false;
	if (!fmt_key_equal(&a->fmt_spec, &b->fmt_spec, b_fam)) return false;
	return memcmp(layout_text_ptr(a), b_txt, a->text_len) == 0;
}

static void lru_unlink_lay(FluxTextCaches *c, int16_t idx) {
	FluxLayoutCacheEntry *e = &c->layout_pool [idx];
	if (e->lru_prev >= 0) c->layout_pool [e->lru_prev].lru_next = e->lru_next;
	else c->lay_head = e->lru_next;
	if (e->lru_next >= 0) c->layout_pool [e->lru_next].lru_prev = e->lru_prev;
	else c->lay_tail = e->lru_prev;
	e->lru_prev = e->lru_next = -1;
}

static void lru_push_front_lay(FluxTextCaches *c, int16_t idx) {
	FluxLayoutCacheEntry *e = &c->layout_pool [idx];
	e->lru_prev             = -1;
	e->lru_next             = c->lay_head;
	if (c->lay_head >= 0) c->layout_pool [c->lay_head].lru_prev = idx;
	else c->lay_tail = idx;
	c->lay_head = idx;
}

static void bucket_unlink_lay(FluxTextCaches *c, int16_t idx) {
	uint32_t b   = ( uint32_t ) c->layout_pool [idx].key.hash & (FLUX_LAYOUT_HASH_CAP - 1u);
	int16_t *cur = &c->layout_buckets [b];
	while (*cur >= 0) {
		if (*cur == idx) {
			*cur                           = c->layout_pool [idx].hash_next;
			c->layout_pool [idx].hash_next = -1;
			return;
		}
		cur = &c->layout_pool [*cur].hash_next;
	}
}

static int16_t alloc_lay_slot(FluxTextCaches *c) {
	if (c->lay_count < FLUX_LAYOUT_CACHE_CAP) return ( int16_t ) c->lay_count++;
	int16_t victim = c->lay_tail;
	lru_unlink_lay(c, victim);
	bucket_unlink_lay(c, victim);
	FluxLayoutCacheEntry *e = &c->layout_pool [victim];
	if (e->layout) IDWriteTextLayout_Release(e->layout);
	layout_key_free(&e->key);
	e->layout = NULL;
	e->in_use = false;
	return victim;
}

IDWriteTextLayout *
get_or_create_layout_ex(FluxText *tr, TextLayoutRequest const *req, DWRITE_WORD_WRAPPING wrap, int *out_wlen) {
	FluxLayoutCacheKey spec;
	char const        *txt = NULL, *fam = NULL;
	make_layout_spec(req, wrap, &spec, &txt, &fam);
	FluxTextCaches *c = text_ensure_caches(tr);
	uint32_t        b = ( uint32_t ) spec.hash & (FLUX_LAYOUT_HASH_CAP - 1u);
	for (int16_t i = c ? c->layout_buckets [b] : -1; i >= 0; i = c->layout_pool [i].hash_next) {
		if (!layout_key_equal(&c->layout_pool [i].key, &spec, txt, fam)) continue;
		lru_unlink_lay(c, i);
		lru_push_front_lay(c, i);
		if (out_wlen) *out_wlen = c->layout_pool [i].key.wlen;
		return c->layout_pool [i].layout;
	}
	IDWriteTextFormat *fmt = get_or_create_format_ex(tr, req->style, wrap);
	if (!fmt || !c) return NULL;
	FluxWideBuffer wide;
	if (!flux_wide_buffer_from_utf8(&wide, txt)) return NULL;
	IDWriteTextLayout *layout = NULL;
	HRESULT            hr     = IDWriteFactory_CreateTextLayout(
	  tr->factory, wide.text, ( UINT32 ) wide.len, fmt, req->max_w, req->max_h, &layout
	);
	int wlen = wide.len;
	flux_wide_buffer_free(&wide);
	if (FAILED(hr) || !layout) return NULL;
	text_configure_layout(tr, layout, txt, req->style);
	if (out_wlen) *out_wlen = wlen;
	spec.wlen = wlen;
	if (!layout_key_init_text(&spec, txt, spec.text_len, fam)) {
		IDWriteTextLayout_Release(layout);
		return NULL;
	}
	int16_t slot                    = alloc_lay_slot(c);
	c->layout_pool [slot].key       = spec;
	c->layout_pool [slot].layout    = layout;
	c->layout_pool [slot].in_use    = true;
	c->layout_pool [slot].hash_next = c->layout_buckets [b];
	c->layout_buckets [b]           = slot;
	lru_push_front_lay(c, slot);
	return layout;
}
