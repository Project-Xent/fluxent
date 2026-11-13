#ifndef FLUX_TEXT_INTERNAL_H
#define FLUX_TEXT_INTERNAL_H

#include "fluxent/flux_text.h"

#include "text/flux_utf.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <cdwrite.h>

#define FLUX_FORMAT_CACHE_SIZE   16
#define FLUX_LAYOUT_CACHE_SIZE   64

#define FLUX_DEFAULT_FONT_FAMILY L"Segoe UI Variable"
#define FLUX_FAMILY_BUF          128

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
	uint32_t style_hash;
	uint32_t max_w_bits;
	uint32_t max_h_bits;
} FluxLayoutCacheKey;

typedef struct FluxLayoutCacheEntry {
	FluxLayoutCacheKey key;
	IDWriteTextLayout *layout;
	uint64_t           last_used;
	bool               occupied;
} FluxLayoutCacheEntry;

typedef struct TextLayoutCacheRequest {
	wchar_t const       *wtext;
	int                  wlen;
	char const          *utf8_text;
	FluxTextStyle const *style;
	float                max_w;
	float                max_h;
} TextLayoutCacheRequest;

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

struct FluxText {
	IDWriteFactory       *factory;
	wchar_t              *default_font;
	float                 default_size;
	FluxFormatCacheEntry  format_cache [FLUX_FORMAT_CACHE_SIZE];
	uint64_t              cache_tick;
	FluxLayoutCacheEntry  layout_cache [FLUX_LAYOUT_CACHE_SIZE];
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

IDWriteTextLayout *get_or_create_layout(FluxText *tr, TextLayoutCacheRequest const *req);

#endif
