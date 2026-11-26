#ifndef FLUX_TEXT_INTERNAL_H
#define FLUX_TEXT_INTERNAL_H

#include "fluxent/flux_text.h"

#include "text/flux_utf.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <cdwrite.h>

#define FLUX_FORMAT_CACHE_CAP    64
#define FLUX_FORMAT_HASH_CAP     128
#define FLUX_LAYOUT_CACHE_CAP    512
#define FLUX_LAYOUT_HASH_CAP     1024

#define FLUX_DEFAULT_FONT_FAMILY L"Segoe UI Variable"
#define FLUX_FAMILY_BUF          128

typedef struct FluxFormatCacheKey {
	uint64_t             hash;
	char                 family_inline [48];
	char                *family_heap;
	uint32_t             family_len;
	uint32_t             size_bits;
	int                  font_weight;
	int                  text_align;
	int                  vert_align;
	DWRITE_WORD_WRAPPING wrapping;
	int                  rtl;
} FluxFormatCacheKey;

typedef struct FluxFormatCacheEntry {
	FluxFormatCacheKey key;
	IDWriteTextFormat *format;
	int16_t            hash_next;
	int16_t            lru_prev;
	int16_t            lru_next;
	bool               in_use;
} FluxFormatCacheEntry;

typedef struct FluxLayoutCacheKey {
	uint64_t           hash;
	uint64_t           format_hash;
	uint64_t           style_hash;
	char               text_inline [64];
	char              *text_heap;
	uint32_t           text_len;
	int                wlen;
	uint32_t           run_count;
	XentTextTrim       trim;
	uint32_t           max_lines;
	uint32_t           max_w_bits;
	uint32_t           max_h_bits;
	FluxFormatCacheKey fmt_spec;
} FluxLayoutCacheKey;

typedef struct FluxLayoutCacheEntry {
	FluxLayoutCacheKey key;
	IDWriteTextLayout *layout;
	int16_t            hash_next;
	int16_t            lru_prev;
	int16_t            lru_next;
	bool               in_use;
} FluxLayoutCacheEntry;

typedef struct TextLayoutRequest {
	FluxText            *tr;
	char const          *text;
	FluxTextStyle const *style;
	float                max_w;
	float                max_h;
} TextLayoutRequest;

typedef struct FluxFontEntry {
	IDWriteFontFace *face;
	XentFontId       id;
} FluxFontEntry;

typedef struct FluxTextCaches {
	FluxFormatCacheEntry format_pool [FLUX_FORMAT_CACHE_CAP];
	int16_t              format_buckets [FLUX_FORMAT_HASH_CAP];
	int16_t              fmt_head;
	int16_t              fmt_tail;
	uint16_t             fmt_count;

	FluxLayoutCacheEntry layout_pool [FLUX_LAYOUT_CACHE_CAP];
	int16_t              layout_buckets [FLUX_LAYOUT_HASH_CAP];
	int16_t              lay_head;
	int16_t              lay_tail;
	uint16_t             lay_count;
} FluxTextCaches;

struct FluxText {
	uint32_t              refcount;
	IDWriteFactory       *factory;
	wchar_t              *default_font;
	float                 default_size;
	uint64_t              cache_tick;
	FluxTextCaches       *caches;
	ID2D1SolidColorBrush *shared_brush;
	ID2D1RenderTarget    *brush_rt;
	FluxFontEntry        *fonts;
	uint32_t              font_count;
	XentFontId            next_font_id;
	XentTextBackend       backend;
};

typedef struct TextLineMetrics {
	DWRITE_LINE_METRICS  stack [8];
	DWRITE_LINE_METRICS *lines;
	UINT32               count;
} TextLineMetrics;

DWRITE_WORD_WRAPPING       policy_to_dwrite_wrapping(XentLineBreakPolicy policy, XentMeasureMode mode);
float                      constraint_for_mode(float width_constraint, XentMeasureMode mode);
DWRITE_TEXT_ALIGNMENT      to_dw_align(FluxTextAlign a);
DWRITE_PARAGRAPH_ALIGNMENT to_dw_para(FluxTextVAlign a);
bool                       read_line_metrics(IDWriteTextLayout *layout, TextLineMetrics *out);
void                       free_line_metrics(TextLineMetrics *m);
float                      kept_line_height(TextLineMetrics const *m, uint32_t max_lines);
void text_configure_layout(FluxText *tr, IDWriteTextLayout *layout, char const *utf8, FluxTextStyle const *style);

void flux_text_caches_release(FluxText *tr);
IDWriteTextFormat *get_or_create_format_ex(FluxText *tr, FluxTextStyle const *s, DWRITE_WORD_WRAPPING wrap);
IDWriteTextLayout *
get_or_create_layout_ex(FluxText *tr, TextLayoutRequest const *req, DWRITE_WORD_WRAPPING wrap, int *out_wlen);
uint32_t flux_text_selection_for_height(FluxTextSelectionQuery const *query, float height, uint32_t *total);

#endif
