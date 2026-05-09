#include "fluxent/flux_text.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <cdwrite.h>

#define FLUX_FORMAT_CACHE_SIZE 16
#define FLUX_LAYOUT_CACHE_SIZE 64

typedef struct FluxFormatCacheKey {
	uint32_t family_hash;
	uint32_t size_bits;
	int      font_weight;
	int      text_align;
	int      vert_align;
	int      word_wrap;
} FluxFormatCacheKey;

typedef struct FluxFormatCacheEntry {
	FluxFormatCacheKey key;
	IDWriteTextFormat *format;
	uint64_t           last_used;
	bool               occupied;
} FluxFormatCacheEntry;

typedef struct FluxLayoutCacheKey {
	uint32_t text_hash;
	uint32_t text_len;
	uint32_t format_hash;
	uint32_t max_w_bits;
	uint32_t max_h_bits;
} FluxLayoutCacheKey;

typedef struct FluxLayoutCacheEntry {
	FluxLayoutCacheKey key;
	IDWriteTextLayout *layout;
	uint64_t           last_used;
	bool               occupied;
} FluxLayoutCacheEntry;

typedef struct TextWideBuffer {
	wchar_t  stack [256];
	wchar_t *text;
	int      len;
} TextWideBuffer;

typedef struct TextLayoutCacheRequest {
	wchar_t const       *wtext;
	int                  wlen;
	char const          *utf8_text;
	FluxTextStyle const *style;
	float                max_w;
	float                max_h;
} TextLayoutCacheRequest;

typedef struct TextLayoutRequest {
	FluxTextRenderer    *tr;
	char const          *text;
	FluxTextStyle const *style;
	float                max_w;
	float                max_h;
} TextLayoutRequest;

struct FluxTextRenderer {
	IDWriteFactory       *factory;
	IDWriteTextFormat    *default_format;
	wchar_t              *default_font;
	float                 default_size;
	FluxFormatCacheEntry  format_cache [FLUX_FORMAT_CACHE_SIZE];
	uint64_t              cache_tick;
	FluxLayoutCacheEntry  layout_cache [FLUX_LAYOUT_CACHE_SIZE];
	ID2D1SolidColorBrush *shared_brush;
	ID2D1RenderTarget    *brush_rt;
};

static uint32_t fnv1a_str(char const *s) {
	uint32_t h = 0x811c9dc5u;
	if (!s) return h;
	while (*s) {
		h ^= ( uint8_t ) *s++;
		h *= 0x01000193u;
	}
	return h;
}

static uint32_t fnv1a_bytes(void const *data, size_t len) {
	uint8_t const *p = ( uint8_t const * ) data;
	uint32_t       h = 0x811c9dc5u;
	for (size_t i = 0; i < len; i++) {
		h ^= p [i];
		h *= 0x01000193u;
	}
	return h;
}

static uint32_t float_to_bits(float f) {
	uint32_t bits;
	memcpy(&bits, &f, sizeof(bits));
	return bits;
}

static int flux_utf8_to_wchar(char const *utf8, wchar_t *out, int out_cap) {
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

static bool text_wide_buffer_from_utf8(TextWideBuffer *buf, char const *utf8) {
	memset(buf, 0, sizeof(*buf));
	int n = flux_utf8_to_wchar(utf8, NULL, 0);
	if (n <= 0) return false;

	buf->text = (n <= ( int ) (sizeof(buf->stack) / sizeof(buf->stack [0])))
	            ? buf->stack
	            : ( wchar_t * ) malloc(sizeof(wchar_t) * n);
	if (!buf->text) return false;

	flux_utf8_to_wchar(utf8, buf->text, n);
	buf->len = n > 0 ? n - 1 : 0;
	return true;
}

static void text_wide_buffer_free(TextWideBuffer *buf) {
	if (buf->text && buf->text != buf->stack) free(buf->text);
	buf->text = NULL;
	buf->len  = 0;
}

static DWRITE_WORD_WRAPPING policy_to_dwrite_wrapping(XentLineBreakPolicy policy, XentMeasureMode mode) {
	if (mode == XENT_MEASURE_UNDEFINED) return DWRITE_WORD_WRAPPING_NO_WRAP;
	switch (policy) {
	case XENT_LINE_BREAK_NO_WRAP   : return DWRITE_WORD_WRAPPING_NO_WRAP;
	case XENT_LINE_BREAK_WORD_WRAP : return DWRITE_WORD_WRAPPING_WRAP;
	case XENT_LINE_BREAK_CHAR_WRAP : return DWRITE_WORD_WRAPPING_CHARACTER;
	default                        : return DWRITE_WORD_WRAPPING_NO_WRAP;
	}
}

static float constraint_for_mode(float width_constraint, XentMeasureMode mode) {
	switch (mode) {
	case XENT_MEASURE_AT_MOST :
	case XENT_MEASURE_EXACTLY : return width_constraint;
	default                   : return 100000.0f;
	}
}

static DWRITE_TEXT_ALIGNMENT to_dw_align(FluxTextAlign a) {
	switch (a) {
	case FLUX_TEXT_CENTER  : return DWRITE_TEXT_ALIGNMENT_CENTER;
	case FLUX_TEXT_RIGHT   : return DWRITE_TEXT_ALIGNMENT_TRAILING;
	case FLUX_TEXT_JUSTIFY : return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
	default                : return DWRITE_TEXT_ALIGNMENT_LEADING;
	}
}

static DWRITE_PARAGRAPH_ALIGNMENT to_dw_para(FluxTextVAlign a) {
	switch (a) {
	case FLUX_TEXT_VCENTER : return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	case FLUX_TEXT_BOTTOM  : return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
	default                : return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
	}
}

static IDWriteTextFormat *create_styled_format(FluxTextRenderer *tr, FluxTextStyle const *s) {
	wchar_t const *family = L"Segoe UI Variable";
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

#define DEFINE_TEXT_CACHE_ACCESSORS(                                                                        \
  Name, EntryType, KeyType, ValueType, cache_field, cache_size, value_field, release_fn                     \
)                                                                                                           \
	static ValueType *find_cached_##Name(FluxTextRenderer *tr, KeyType const *key) {                        \
		for (int i = 0; i < cache_size; i++) {                                                              \
			EntryType *e = &tr->cache_field [i];                                                            \
			if (!e->occupied || memcmp(&e->key, key, sizeof(*key)) != 0) continue;                          \
			e->last_used = tr->cache_tick;                                                                  \
			return e->value_field;                                                                          \
		}                                                                                                   \
		return NULL;                                                                                        \
	}                                                                                                       \
	static int find_##Name##_cache_slot(FluxTextRenderer *tr) {                                             \
		int      slot   = 0;                                                                                \
		uint64_t oldest = UINT64_MAX;                                                                       \
		for (int i = 0; i < cache_size; i++) {                                                              \
			if (!tr->cache_field [i].occupied) return i;                                                    \
			if (tr->cache_field [i].last_used >= oldest) continue;                                          \
			oldest = tr->cache_field [i].last_used;                                                         \
			slot   = i;                                                                                     \
		}                                                                                                   \
		return slot;                                                                                        \
	}                                                                                                       \
	static void store_cached_##Name(FluxTextRenderer *tr, int slot, KeyType const *key, ValueType *value) { \
		if (tr->cache_field [slot].occupied && tr->cache_field [slot].value_field)                          \
			release_fn(tr->cache_field [slot].value_field);                                                 \
		tr->cache_field [slot].key         = *key;                                                          \
		tr->cache_field [slot].value_field = value;                                                         \
		tr->cache_field [slot].last_used   = tr->cache_tick;                                                \
		tr->cache_field [slot].occupied    = true;                                                          \
	}

DEFINE_TEXT_CACHE_ACCESSORS(
  format, FluxFormatCacheEntry, FluxFormatCacheKey, IDWriteTextFormat, format_cache, FLUX_FORMAT_CACHE_SIZE, format,
  IDWriteTextFormat_Release
)

static IDWriteTextFormat *get_or_create_format(FluxTextRenderer *tr, FluxTextStyle const *s) {
	FluxFormatCacheKey key = make_format_key(s);
	IDWriteTextFormat *fmt = find_cached_format(tr, &key);
	if (fmt) return fmt;

	fmt = create_styled_format(tr, s);
	if (!fmt) return NULL;
	store_cached_format(tr, find_format_cache_slot(tr), &key, fmt);
	return fmt;
}

static FluxLayoutCacheKey make_layout_key(char const *utf8_text, FluxTextStyle const *s, float max_w, float max_h) {
	FluxFormatCacheKey fk = make_format_key(s);
	FluxLayoutCacheKey key;
	key.text_hash   = fnv1a_str(utf8_text);
	key.text_len    = ( uint32_t ) (utf8_text ? strlen(utf8_text) : 0);
	key.format_hash = fnv1a_bytes(&fk, sizeof(fk));
	key.max_w_bits  = float_to_bits(max_w);
	key.max_h_bits  = float_to_bits(max_h);
	return key;
}

DEFINE_TEXT_CACHE_ACCESSORS(
  layout, FluxLayoutCacheEntry, FluxLayoutCacheKey, IDWriteTextLayout, layout_cache, FLUX_LAYOUT_CACHE_SIZE, layout,
  IDWriteTextLayout_Release
)

static IDWriteTextLayout *get_or_create_layout(FluxTextRenderer *tr, TextLayoutCacheRequest const *req) {
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
	store_cached_layout(tr, find_layout_cache_slot(tr), &key, layout);
	return layout;
}

static IDWriteTextFormat *
dwrite_create_measure_format(FluxTextRenderer *tr, float font_size, XentLineBreakPolicy policy, XentMeasureMode mode) {
	IDWriteTextFormat *fmt = NULL;
	HRESULT            hr  = IDWriteFactory_CreateTextFormat(
	  tr->factory, tr->default_font ? tr->default_font : L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_REGULAR,
	  DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, font_size, L"", &fmt
	);
	if (FAILED(hr)) return NULL;

	IDWriteTextFormat_SetWordWrapping(fmt, policy_to_dwrite_wrapping(policy, mode));
	return fmt;
}

static IDWriteTextLayout *
dwrite_create_measure_layout(FluxTextRenderer *tr, IDWriteTextFormat *fmt, TextWideBuffer const *text, float width) {
	IDWriteTextLayout *layout = NULL;
	HRESULT            hr
	  = IDWriteFactory_CreateTextLayout(tr->factory, text->text, ( UINT32 ) text->len, fmt, width, 100000.0f, &layout);
	if (FAILED(hr)) return NULL;
	return layout;
}

static bool
dwrite_read_metrics(IDWriteTextLayout *layout, XentMeasureMode mode, float width_constraint, XentTextMetrics *out) {
	DWRITE_TEXT_METRICS metrics;
	HRESULT             hr = IDWriteTextLayout_GetMetrics(layout, &metrics);
	if (FAILED(hr)) return false;

	out->width      = metrics.widthIncludingTrailingWhitespace;
	out->height     = metrics.height;
	out->line_count = metrics.lineCount;
	if (mode == XENT_MEASURE_EXACTLY) out->width = width_constraint;
	return true;
}

static bool dwrite_measure(
  XentTextBackend const *backend, char const *text, float font_size, float width_constraint, XentLineBreakPolicy policy,
  XentMeasureMode mode, XentTextMetrics *out
) {
	FluxTextRenderer *tr = ( FluxTextRenderer * ) backend->userdata;
	if (!tr || !out) return false;

	TextWideBuffer wide;
	if (!text_wide_buffer_from_utf8(&wide, text)) return false;

	IDWriteTextFormat *fmt = dwrite_create_measure_format(tr, font_size, policy, mode);
	if (!fmt) {
		text_wide_buffer_free(&wide);
		return false;
	}

	float              layout_width = constraint_for_mode(width_constraint, mode);
	IDWriteTextLayout *layout       = dwrite_create_measure_layout(tr, fmt, &wide, layout_width);
	text_wide_buffer_free(&wide);

	bool ok = layout && dwrite_read_metrics(layout, mode, width_constraint, out);

	if (layout) IDWriteTextLayout_Release(layout);
	IDWriteTextFormat_Release(fmt);
	return ok;
}

static bool dwrite_shape(
  XentTextBackend const *backend, char const *text, float font_size, float width_constraint, XentLineBreakPolicy policy,
  XentMeasureMode mode, XentShapedGlyph *out_glyphs, uint32_t glyph_capacity, XentShapedRun *out_runs,
  uint32_t run_capacity, XentShapedLine *out_lines, uint32_t line_capacity, XentShapingResult *out_result
) {
	if (!out_result) return false;

	XentTextMetrics m;
	if (!dwrite_measure(backend, text, font_size, width_constraint, policy, mode, &m)) return false;

	memset(out_result, 0, sizeof(*out_result));
	out_result->metrics     = m;
	out_result->glyph_count = 0;
	out_result->run_count   = 0;
	out_result->line_count  = m.line_count;
	out_result->truncated   = false;
	return true;
}

static XentTextBackend const kDWriteBackend = {
  .name     = "fluxent-dwrite",
  .measure  = dwrite_measure,
  .shape    = dwrite_shape,
  .userdata = NULL,
};

FluxTextRenderer *flux_text_renderer_create(void) {
	FluxTextRenderer *tr = ( FluxTextRenderer * ) calloc(1, sizeof(*tr));
	if (!tr) return NULL;

	HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, ( void ** ) &tr->factory);
	if (FAILED(hr)) {
		free(tr);
		return NULL;
	}

	tr->default_size = 14.0f;
	return tr;
}

void flux_text_renderer_destroy(FluxTextRenderer *tr) {
	if (!tr) return;

	for (int i = 0; i < FLUX_FORMAT_CACHE_SIZE; i++)
		if (tr->format_cache [i].occupied && tr->format_cache [i].format)
			IDWriteTextFormat_Release(tr->format_cache [i].format);

	for (int i = 0; i < FLUX_LAYOUT_CACHE_SIZE; i++)
		if (tr->layout_cache [i].occupied && tr->layout_cache [i].layout)
			IDWriteTextLayout_Release(tr->layout_cache [i].layout);

	if (tr->shared_brush) ID2D1SolidColorBrush_Release(tr->shared_brush);

	if (tr->factory) IDWriteFactory_Release(tr->factory);
	free(tr->default_font);
	free(tr);
}

bool flux_text_renderer_register(FluxTextRenderer *tr, XentContext *ctx) {
	if (!tr || !ctx) return false;

	XentTextBackend backend = kDWriteBackend;
	backend.userdata        = tr;

	return xent_set_text_backend(ctx, &backend);
}

#define FLUX_TEXT_UNBOUNDED 100000.0f

static IDWriteTextLayout *text_layout_from_utf8(TextLayoutRequest const *req, TextWideBuffer *wide) {
	if (!text_wide_buffer_from_utf8(wide, req->text)) return NULL;

	TextLayoutCacheRequest cache_req = {wide->text, wide->len, req->text, req->style, req->max_w, req->max_h};
	IDWriteTextLayout     *layout    = get_or_create_layout(req->tr, &cache_req);
	if (!layout) text_wide_buffer_free(wide);
	return layout;
}

static bool text_ensure_shared_brush(FluxTextRenderer *tr, ID2D1RenderTarget *rt, FluxColor color) {
	D2D1_COLOR_F cf;
	cf.r = flux_color_rf(color);
	cf.g = flux_color_gf(color);
	cf.b = flux_color_bf(color);
	cf.a = flux_color_af(color);

	if (tr->shared_brush && tr->brush_rt != rt) {
		ID2D1SolidColorBrush_Release(tr->shared_brush);
		tr->shared_brush = NULL;
		tr->brush_rt     = NULL;
	}
	if (!tr->shared_brush) {
		D2D1_BRUSH_PROPERTIES bp;
		memset(&bp, 0, sizeof(bp));
		bp.opacity       = 1.0f;
		bp.transform._11 = 1.0f;
		bp.transform._22 = 1.0f;
		HRESULT hr       = ID2D1RenderTarget_CreateSolidColorBrush(rt, &cf, &bp, &tr->shared_brush);
		if (FAILED(hr)) return false;
		tr->brush_rt = rt;
	}
	else { ID2D1SolidColorBrush_SetColor(tr->shared_brush, &cf); }
	return true;
}

static float text_effective_max_width(float max_width) { return max_width > 0.0f ? max_width : FLUX_TEXT_UNBOUNDED; }

static TextLayoutRequest text_layout_request(FluxTextLayoutQuery const *query, char const *text) {
	return (TextLayoutRequest) {
	  query->renderer,
	  text ? text : query->text,
	  query->style,
	  text_effective_max_width(query->max_width),
	  FLUX_TEXT_UNBOUNDED,
	};
}

void flux_text_draw(
  FluxTextRenderer *tr, ID2D1RenderTarget *rt, char const *text, FluxRect const *bounds, FluxTextStyle const *style
) {
	if (!tr || !rt || !text || !text [0] || !bounds || !style) return;

	tr->cache_tick++;

	TextWideBuffer     wide;
	TextLayoutRequest  req    = {tr, text, style, bounds->w, bounds->h};
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wide);
	if (!layout) return;
	if (!text_ensure_shared_brush(tr, rt, style->color)) {
		text_wide_buffer_free(&wide);
		return;
	}

	D2D1_POINT_2F origin;
	origin.x = bounds->x;
	origin.y = bounds->y;
	ID2D1RenderTarget_DrawTextLayout(
	  rt, origin, layout, ( ID2D1Brush * ) tr->shared_brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
	);
	text_wide_buffer_free(&wide);
}

FluxSize flux_text_measure(FluxTextRenderer *tr, char const *text, FluxTextStyle const *style, float max_width) {
	FluxSize result = {0, 0};
	if (!tr || !text || !text [0] || !style) return result;

	tr->cache_tick++;

	TextWideBuffer     wide;
	float              mw     = text_effective_max_width(max_width);
	TextLayoutRequest  req    = {tr, text, style, mw, FLUX_TEXT_UNBOUNDED};
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wide);
	if (!layout) return result;

	DWRITE_TEXT_METRICS metrics;
	if (SUCCEEDED(IDWriteTextLayout_GetMetrics(layout, &metrics))) {
		result.w = metrics.widthIncludingTrailingWhitespace;
		result.h = metrics.height;
	}

	text_wide_buffer_free(&wide);
	return result;
}

int flux_text_hit_test(FluxTextHitTestQuery const *query) {
	if (!query || !query->layout.renderer || !query->layout.text || !query->layout.text [0] || !query->layout.style)
		return 0;

	query->layout.renderer->cache_tick++;

	TextWideBuffer     wide;
	TextLayoutRequest  req    = text_layout_request(&query->layout, NULL);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wide);
	if (!layout) return 0;

	BOOL                    is_trailing, is_inside;
	DWRITE_HIT_TEST_METRICS htm;
	int                     pos = 0;
	if (SUCCEEDED(IDWriteTextLayout_HitTestPoint(layout, query->x, query->y, &is_trailing, &is_inside, &htm)))
		pos = is_trailing ? ( int ) htm.textPosition + 1 : ( int ) htm.textPosition;

	text_wide_buffer_free(&wide);
	return pos;
}

FluxRect flux_text_caret_rect(FluxTextCaretQuery const *query) {
	FluxRect result = {0, 0, 1.0f, 0};
	if (!query || !query->layout.renderer || !query->layout.style) return result;

	query->layout.renderer->cache_tick++;

	char const        *safe = (query->layout.text && query->layout.text [0]) ? query->layout.text : " ";
	TextWideBuffer     wide;
	TextLayoutRequest  req    = text_layout_request(&query->layout, safe);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wide);
	if (!layout) return result;

	int index = query->index;
	if (index < 0) index = 0;
	if (index > wide.len) index = wide.len;

	float                   cx, cy;
	DWRITE_HIT_TEST_METRICS htm;
	if (SUCCEEDED(IDWriteTextLayout_HitTestTextPosition(layout, ( UINT32 ) index, FALSE, &cx, &cy, &htm))) {
		result.x = cx;
		result.y = cy;
		result.w = 1.0f;
		result.h = htm.height;
	}

	text_wide_buffer_free(&wide);
	return result;
}

static bool text_clamp_selection_range(int *start, int *end, int len) {
	if (*start < 0) *start = 0;
	if (*end > len) *end = len;
	return *start < *end;
}

static DWRITE_HIT_TEST_METRICS *
text_selection_metrics(IDWriteTextLayout *layout, int start, int end, UINT32 *out_count) {
	*out_count = 0;
	IDWriteTextLayout_HitTestTextRange(layout, ( UINT32 ) start, ( UINT32 ) (end - start), 0, 0, NULL, 0, out_count);
	if (*out_count == 0) return NULL;

	DWRITE_HIT_TEST_METRICS *metrics
	  = ( DWRITE_HIT_TEST_METRICS * ) malloc(sizeof(DWRITE_HIT_TEST_METRICS) * *out_count);
	if (!metrics) {
		*out_count = 0;
		return NULL;
	}

	IDWriteTextLayout_HitTestTextRange(
	  layout, ( UINT32 ) start, ( UINT32 ) (end - start), 0, 0, metrics, *out_count, out_count
	);
	return metrics;
}

static uint32_t text_copy_selection_rects(
  DWRITE_HIT_TEST_METRICS const *metrics, UINT32 count, FluxRect *out_rects, uint32_t max_rects
) {
	uint32_t n = count < max_rects ? count : max_rects;
	for (uint32_t i = 0; i < n; i++) {
		out_rects [i].x = metrics [i].left;
		out_rects [i].y = metrics [i].top;
		out_rects [i].w = metrics [i].width;
		out_rects [i].h = metrics [i].height;
	}
	return n;
}

uint32_t flux_text_selection_rects(FluxTextSelectionQuery const *query) {
	if (!query
		|| !query->layout.renderer
		|| !query->layout.text
		|| !query->layout.text [0]
		|| !query->layout.style
		|| !query->out_rects
		|| query->max_rects == 0)
		return 0;
	if (query->start >= query->end) return 0;

	query->layout.renderer->cache_tick++;

	TextWideBuffer     wide;
	TextLayoutRequest  req    = text_layout_request(&query->layout, NULL);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wide);
	if (!layout) return 0;

	int start = query->start;
	int end   = query->end;
	if (!text_clamp_selection_range(&start, &end, wide.len)) {
		text_wide_buffer_free(&wide);
		return 0;
	}

	UINT32                   count = 0;
	DWRITE_HIT_TEST_METRICS *htm   = text_selection_metrics(layout, start, end, &count);
	if (!htm) {
		text_wide_buffer_free(&wide);
		return 0;
	}

	uint32_t n = text_copy_selection_rects(htm, count, query->out_rects, query->max_rects);

	free(htm);
	text_wide_buffer_free(&wide);
	return n;
}
