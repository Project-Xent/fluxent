#include "text/flux_text_internal.h"

#include <stdlib.h>
#include <string.h>

FluxText *flux_text_create(void) {
	FluxText *tr = ( FluxText * ) calloc(1, sizeof(*tr));
	if (!tr) return NULL;

	HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, ( void ** ) &tr->factory);
	if (FAILED(hr)) {
		free(tr);
		return NULL;
	}

	tr->refcount     = 1u;
	tr->default_size = 14.0f;
	return tr;
}

void flux_text_retain(FluxText *tr) {
	if (tr) tr->refcount++;
}

static void release_fonts(FluxText *tr) {
	for (uint32_t i = 0u; i < tr->font_count; i++) IDWriteFontFace_Release(tr->fonts [i].face);
	free(tr->fonts);
	tr->fonts      = NULL;
	tr->font_count = 0u;
}

void flux_text_destroy(FluxText *tr) {
	if (!tr) return;
	if (tr->refcount > 1u) {
		tr->refcount--;
		return;
	}

	flux_text_caches_release(tr);
	release_fonts(tr);

	if (tr->shared_brush) ID2D1SolidColorBrush_Release(tr->shared_brush);
	if (tr->factory) IDWriteFactory_Release(tr->factory);
	free(tr->default_font);
	free(tr);
}

#define FLUX_TEXT_UNBOUNDED 100000.0f

static IDWriteTextLayout *text_layout_from_utf8(TextLayoutRequest const *req, int *out_wlen) {
	DWRITE_WORD_WRAPPING wrap = req->style->word_wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP;
	return get_or_create_layout_ex(req->tr, req, wrap, out_wlen);
}

static bool text_ensure_shared_brush(FluxText *tr, ID2D1RenderTarget *rt, FluxColor color) {
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
  FluxText *tr, ID2D1RenderTarget *rt, char const *text, FluxRect const *bounds, FluxTextStyle const *style
) {
	if (!tr || !rt || !text || !text [0] || !bounds || !style) return;

	tr->cache_tick++;

	TextLayoutRequest  req    = {tr, text, style, bounds->w, bounds->h};
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, NULL);
	if (!layout || !text_ensure_shared_brush(tr, rt, style->color)) return;

	D2D1_POINT_2F origin;
	origin.x = bounds->x;
	origin.y = bounds->y;
	ID2D1RenderTarget_DrawTextLayout(
	  rt, origin, layout, ( ID2D1Brush * ) tr->shared_brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
	);
}

FluxSize flux_text_measure(FluxText *tr, char const *text, FluxTextStyle const *style, float max_width) {
	FluxSize result = {0, 0};
	if (!tr || !text || !text [0] || !style) return result;

	tr->cache_tick++;

	float              mw     = text_effective_max_width(max_width);
	TextLayoutRequest  req    = {tr, text, style, mw, FLUX_TEXT_UNBOUNDED};
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, NULL);
	if (!layout) return result;

	DWRITE_TEXT_METRICS metrics;
	if (SUCCEEDED(IDWriteTextLayout_GetMetrics(layout, &metrics))) {
		result.w = metrics.widthIncludingTrailingWhitespace;
		result.h = metrics.height;
	}
	return result;
}

int flux_text_hittest(FluxTextHitTestQuery const *query) {
	if (!query || !query->layout.renderer || !query->layout.text || !query->layout.text [0] || !query->layout.style)
		return 0;

	query->layout.renderer->cache_tick++;

	TextLayoutRequest  req    = text_layout_request(&query->layout, NULL);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, NULL);
	if (!layout) return 0;

	BOOL                    is_trailing, is_inside;
	DWRITE_HIT_TEST_METRICS htm;
	int                     pos = 0;
	if (SUCCEEDED(IDWriteTextLayout_HitTestPoint(layout, query->x, query->y, &is_trailing, &is_inside, &htm)))
		pos = is_trailing ? ( int ) htm.textPosition + 1 : ( int ) htm.textPosition;
	return pos;
}

FluxRect flux_text_caret(FluxTextCaretQuery const *query) {
	FluxRect result = {0, 0, 1.0f, 0};
	if (!query || !query->layout.renderer || !query->layout.style) return result;

	query->layout.renderer->cache_tick++;

	char const        *safe   = (query->layout.text && query->layout.text [0]) ? query->layout.text : " ";
	int                wlen   = 0;
	TextLayoutRequest  req    = text_layout_request(&query->layout, safe);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wlen);
	if (!layout) return result;

	int index = query->index;
	if (index < 0) index = 0;
	if (index > wlen) index = wlen;

	float                   cx, cy;
	DWRITE_HIT_TEST_METRICS htm;
	if (SUCCEEDED(IDWriteTextLayout_HitTestTextPosition(layout, ( UINT32 ) index, FALSE, &cx, &cy, &htm))) {
		result.x = cx;
		result.y = cy;
		result.w = 1.0f;
		result.h = htm.height;
	}
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

uint32_t flux_text_selection(FluxTextSelectionQuery const *query) {
	uint32_t total;
	return flux_text_selection_for_height(query, FLUX_TEXT_UNBOUNDED, &total);
}

static bool text_selection_query_valid(FluxTextSelectionQuery const *query) {
	return query
	    && query->layout.renderer
	    && query->layout.text
	    && query->layout.text [0]
	    && query->layout.style
	    && query->out_rects
	    && query->max_rects > 0
	    && query->start < query->end;
}

uint32_t flux_text_selection_for_height(FluxTextSelectionQuery const *query, float height, uint32_t *total) {
	*total = 0;
	if (!text_selection_query_valid(query)) return 0;

	query->layout.renderer->cache_tick++;

	int               wlen    = 0;
	TextLayoutRequest req     = text_layout_request(&query->layout, NULL);
	req.max_h                 = height;
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, &wlen);
	if (!layout) return 0;

	int start = query->start;
	int end   = query->end;
	if (!text_clamp_selection_range(&start, &end, wlen)) return 0;

	DWRITE_HIT_TEST_METRICS *htm = text_selection_metrics(layout, start, end, total);
	uint32_t                 n   = htm ? text_copy_selection_rects(htm, *total, query->out_rects, query->max_rects) : 0;
	free(htm);
	return n;
}

static void text_copy_lines(TextLineMetrics const *m, FluxTextLine *out, uint32_t max_lines) {
	uint32_t start = 0u;
	float    top   = 0.0f;
	for (uint32_t i = 0u; i < m->count && i < max_lines; i++) {
		out [i]  = (FluxTextLine) {start, m->lines [i].length, m->lines [i].newlineLength, top, m->lines [i].height};
		start   += m->lines [i].length;
		top     += m->lines [i].height;
	}
}

uint32_t flux_text_lines(FluxTextLayoutQuery const *query, FluxTextLine *out, uint32_t max_lines) {
	if (!query || !query->renderer || !query->style) return 0u;
	query->renderer->cache_tick++;
	char const        *safe   = query->text && query->text [0] ? query->text : " ";
	TextLayoutRequest  req    = text_layout_request(query, safe);
	IDWriteTextLayout *layout = text_layout_from_utf8(&req, NULL);
	TextLineMetrics    m;
	bool               ok = layout && read_line_metrics(layout, &m);
	uint32_t           n  = ok ? m.count : 0u;
	if (ok && out) text_copy_lines(&m, out, max_lines);
	if (ok) free_line_metrics(&m);
	return n;
}

static IDWriteTextLayout *text_cluster_layout(FluxTextClusterQuery const *query, int *wlen) {
	char const       *safe = (query->layout.text && query->layout.text [0]) ? query->layout.text : " ";
	TextLayoutRequest req  = text_layout_request(&query->layout, safe);
	return text_layout_from_utf8(&req, wlen);
}

uint32_t flux_text_next(FluxTextClusterQuery const *query) {
	if (!query || !query->layout.renderer || !query->layout.style) return query ? query->index : 0;

	query->layout.renderer->cache_tick++;

	int                wlen   = 0;
	IDWriteTextLayout *layout = text_cluster_layout(query, &wlen);
	if (!layout) return query->index;
	if (( int ) query->index >= wlen) return ( uint32_t ) wlen;

	float                   cx, cy;
	DWRITE_HIT_TEST_METRICS htm;
	HRESULT                 hr = IDWriteTextLayout_HitTestTextPosition(layout, query->index, FALSE, &cx, &cy, &htm);
	return SUCCEEDED(hr) ? htm.textPosition + htm.length : query->index + 1;
}

uint32_t flux_text_prev(FluxTextClusterQuery const *query) {
	if (!query || !query->layout.renderer || !query->layout.style) return query ? query->index : 0;
	if (query->index == 0) return 0;

	query->layout.renderer->cache_tick++;

	int                wlen   = 0;
	IDWriteTextLayout *layout = text_cluster_layout(query, &wlen);
	if (!layout) return query->index;

	float                   cx, cy;
	DWRITE_HIT_TEST_METRICS htm;
	HRESULT                 hr = IDWriteTextLayout_HitTestTextPosition(layout, query->index - 1, FALSE, &cx, &cy, &htm);
	return SUCCEEDED(hr) ? htm.textPosition : query->index - 1;
}
