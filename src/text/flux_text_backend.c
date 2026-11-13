#include "text/flux_text_internal.h"

#include <stdlib.h>
#include <string.h>

static IDWriteTextFormat *dwrite_create_measure_format(FluxText *tr, XentTextMeasureReq const *request) {
	wchar_t const *family = tr->default_font ? tr->default_font : FLUX_DEFAULT_FONT_FAMILY;
	wchar_t        family_buf [FLUX_FAMILY_BUF];
	if (request->font_family
	    && request->font_family [0]
	    && flux_utf8_to_wchar(request->font_family, family_buf, FLUX_FAMILY_BUF) > 0)
		family = family_buf;

	DWRITE_FONT_WEIGHT weight = ( DWRITE_FONT_WEIGHT ) (request->font_weight ? request->font_weight : 400);
	IDWriteTextFormat *fmt    = NULL;
	HRESULT            hr     = IDWriteFactory_CreateTextFormat(
	  tr->factory, family, NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, request->font_size, L"",
	  &fmt
	);
	if (FAILED(hr)) return NULL;

	IDWriteTextFormat_SetWordWrapping(fmt, policy_to_dwrite_wrapping(request->line_break_policy, request->width_mode));
	return fmt;
}

static FluxTextStyle measure_style_from_request(XentTextMeasureReq const *request) {
	bool constrained = request->width_mode == XENT_MEASURE_AT_MOST || request->width_mode == XENT_MEASURE_EXACTLY;

	FluxTextStyle style;
	memset(&style, 0, sizeof(style));
	style.runs      = request->runs;
	style.run_count = request->run_count;
	style.trim      = constrained ? request->trim : XENT_TEXT_TRIM_NONE;
	style.max_lines = constrained ? request->max_lines : 0u;
	return style;
}

static IDWriteTextLayout *
dwrite_create_measure_layout(FluxText *tr, IDWriteTextFormat *fmt, FluxWideBuffer const *text, float width) {
	IDWriteTextLayout *layout = NULL;
	HRESULT            hr
	  = IDWriteFactory_CreateTextLayout(tr->factory, text->text, ( UINT32 ) text->len, fmt, width, 100000.0f, &layout);
	if (FAILED(hr)) return NULL;
	return layout;
}

static float trimmed_width(float width, XentTextMeasureReq const *request) {
	if (request->trim == XENT_TEXT_TRIM_NONE) return width;
	if (request->width_mode != XENT_MEASURE_AT_MOST && request->width_mode != XENT_MEASURE_EXACTLY) return width;
	return width > request->width_constraint ? request->width_constraint : width;
}

static void read_line_derived_metrics(IDWriteTextLayout *layout, uint32_t max_lines, XentTextMetrics *out) {
	TextLineMetrics m;
	if (!read_line_metrics(layout, &m)) return;

	out->first_baseline = m.lines [0].baseline;
	if (max_lines && m.count > max_lines) {
		out->line_count = max_lines;
		out->height     = kept_line_height(&m, max_lines);
	}
	free_line_metrics(&m);
}

static bool dwrite_read_metrics(IDWriteTextLayout *layout, XentTextMeasureReq const *request, XentTextMetrics *out) {
	if (request->width_mode == XENT_MEASURE_MIN_CONTENT) {
		FLOAT min_width = 0.0f;
		if (FAILED(IDWriteTextLayout_DetermineMinWidth(layout, &min_width))) return false;
		if (FAILED(IDWriteTextLayout_SetMaxWidth(layout, min_width))) return false;
	}

	DWRITE_TEXT_METRICS metrics;
	HRESULT             hr = IDWriteTextLayout_GetMetrics(layout, &metrics);
	if (FAILED(hr)) return false;

	out->width      = trimmed_width(metrics.widthIncludingTrailingWhitespace, request);
	out->height     = metrics.height;
	out->line_count = metrics.lineCount;
	read_line_derived_metrics(layout, request->max_lines, out);
	if (request->width_mode == XENT_MEASURE_EXACTLY) out->width = request->width_constraint;
	return true;
}

static IDWriteTextLayout *dwrite_build_measure_layout(FluxText *tr, XentTextMeasureReq const *request) {
	FluxWideBuffer wide;
	if (!flux_wide_buffer_from_utf8(&wide, request->text)) return NULL;

	IDWriteTextFormat *fmt = dwrite_create_measure_format(tr, request);
	if (!fmt) {
		flux_wide_buffer_free(&wide);
		return NULL;
	}

	float              width  = constraint_for_mode(request->width_constraint, request->width_mode);
	IDWriteTextLayout *layout = dwrite_create_measure_layout(tr, fmt, &wide, width);
	flux_wide_buffer_free(&wide);
	IDWriteTextFormat_Release(fmt);

	if (!layout) return NULL;

	FluxTextStyle style = measure_style_from_request(request);
	text_configure_layout(tr, layout, request->text, &style);
	return layout;
}

static bool dwrite_measure(XentTextBackend const *backend, XentTextMeasureReq const *request, XentTextMetrics *out) {
	FluxText *tr = ( FluxText * ) backend->userdata;
	if (!tr || !request || !out) return false;

	IDWriteTextLayout *layout = dwrite_build_measure_layout(tr, request);
	if (!layout) return false;

	bool ok = dwrite_read_metrics(layout, request, out);
	IDWriteTextLayout_Release(layout);
	return ok;
}

static XentFontId font_id_for_face(FluxText *tr, IDWriteFontFace *face) {
	if (!face) return XENT_FONT_INVALID;
	for (uint32_t i = 0u; i < tr->font_count; ++i)
		if (tr->fonts [i].face == face) return tr->fonts [i].id;

	FluxFontEntry *grown = ( FluxFontEntry * ) realloc(tr->fonts, sizeof(*grown) * ( size_t ) (tr->font_count + 1u));
	if (!grown) return XENT_FONT_INVALID;
	tr->fonts = grown;

	IDWriteFontFace_AddRef(face);
	tr->fonts [tr->font_count].face  = face;
	tr->fonts [tr->font_count].id    = ++tr->next_font_id;
	tr->font_count                  += 1u;
	return tr->next_font_id;
}

typedef struct FluxShapeVtbl {
	void *tbl [10];
} FluxShapeVtbl;

typedef struct FluxShapeRenderer {
	FluxShapeVtbl const *v;
	FluxText            *tr;
	XentShapeSink       *sink;
	uint32_t const      *utf16_to_byte;
	uint32_t             wlen;
	bool                 failed;
} FluxShapeRenderer;

static HRESULT WINAPI shape_query_interface(void *self, void const *iid, void **out) {
	( void ) self;
	( void ) iid;
	if (out) *out = NULL;
	return E_NOINTERFACE;
}

static ULONG WINAPI shape_addref(void *self) {
	( void ) self;
	return 1u;
}

static ULONG WINAPI shape_release(void *self) {
	( void ) self;
	return 1u;
}

static HRESULT WINAPI shape_is_pixel_snapping_disabled(void *self, void *ctx, BOOL *disabled) {
	( void ) self;
	( void ) ctx;
	if (disabled) *disabled = TRUE;
	return S_OK;
}

static HRESULT WINAPI shape_get_current_transform(void *self, void *ctx, DWRITE_MATRIX *transform) {
	( void ) self;
	( void ) ctx;
	if (!transform) return E_POINTER;
	memset(transform, 0, sizeof(*transform));
	transform->m11 = 1.0f;
	transform->m22 = 1.0f;
	return S_OK;
}

static HRESULT WINAPI shape_get_pixels_per_dip(void *self, void *ctx, FLOAT *pixels_per_dip) {
	( void ) self;
	( void ) ctx;
	if (pixels_per_dip) *pixels_per_dip = 1.0f;
	return S_OK;
}

static uint32_t shape_cluster_byte(FluxShapeRenderer const *r, uint32_t utf16_offset) {
	if (!r->utf16_to_byte) return 0u;
	return r->utf16_to_byte [utf16_offset <= r->wlen ? utf16_offset : r->wlen];
}

static void shape_fill_clusters(
  FluxShapeRenderer const *r, DWRITE_GLYPH_RUN const *run, DWRITE_GLYPH_RUN_DESCRIPTION const *desc, XentGlyph *glyphs
) {
	uint32_t base = desc ? desc->textPosition : 0u;
	for (uint32_t g = 0u; g < run->glyphCount; ++g) glyphs [g].cluster = shape_cluster_byte(r, base);
	if (!desc || !desc->clusterMap) return;

	for (uint32_t i = desc->stringLength; i-- > 0u;) {
		uint16_t g = desc->clusterMap [i];
		if (g < run->glyphCount) glyphs [g].cluster = shape_cluster_byte(r, base + i);
	}
}

static void shape_fill_glyphs(DWRITE_GLYPH_RUN const *run, XentGlyph *glyphs) {
	for (uint32_t g = 0u; g < run->glyphCount; ++g) {
		glyphs [g].id      = run->glyphIndices [g];
		glyphs [g].advance = run->glyphAdvances ? run->glyphAdvances [g] : 0.0f;
		if (!run->glyphOffsets) continue;
		glyphs [g].offset_x = run->glyphOffsets [g].advanceOffset;
		glyphs [g].offset_y = -run->glyphOffsets [g].ascenderOffset;
	}
}

static HRESULT WINAPI shape_draw_glyph_run(
  void *self, void *ctx, FLOAT baseline_x, FLOAT baseline_y, DWRITE_MEASURING_MODE mode, DWRITE_GLYPH_RUN const *run,
  DWRITE_GLYPH_RUN_DESCRIPTION const *desc, void *effect
) {
	( void ) ctx;
	( void ) mode;
	( void ) effect;

	FluxShapeRenderer *r = ( FluxShapeRenderer * ) self;
	if (!run || run->glyphCount == 0u) return S_OK;

	XentGlyph *glyphs = ( XentGlyph * ) calloc(run->glyphCount, sizeof(*glyphs));
	if (!glyphs) {
		r->failed = true;
		return E_OUTOFMEMORY;
	}

	shape_fill_glyphs(run, glyphs);
	shape_fill_clusters(r, run, desc, glyphs);

	XentGlyphRunDesc emitted = {
	  font_id_for_face(r->tr, run->fontFace),
	  run->fontEmSize,
	  baseline_x,
	  baseline_y,
	  ( uint8_t ) (run->bidiLevel & 0xffu),
	};
	if (!xent_shape_emit(r->sink, &emitted, glyphs, run->glyphCount)) r->failed = true;

	free(glyphs);
	return r->failed ? E_FAIL : S_OK;
}

static HRESULT WINAPI shape_ignore(void *self, void *ctx, FLOAT x, FLOAT y, void *what, void *effect) {
	( void ) self;
	( void ) ctx;
	( void ) x;
	( void ) y;
	( void ) what;
	( void ) effect;
	return S_OK;
}

static HRESULT WINAPI shape_ignore_inline(
  void *self, void *ctx, FLOAT x, FLOAT y, void *obj, BOOL sideways, BOOL rtl, void *effect
) {
	( void ) sideways;
	( void ) rtl;
	return shape_ignore(self, ctx, x, y, obj, effect);
}

static FluxShapeVtbl const kShapeVtbl = {
  {
   ( void * ) shape_query_interface,
   ( void * ) shape_addref,
   ( void * ) shape_release,
   ( void * ) shape_is_pixel_snapping_disabled,
   ( void * ) shape_get_current_transform,
   ( void * ) shape_get_pixels_per_dip,
   ( void * ) shape_draw_glyph_run,
   ( void * ) shape_ignore,
   ( void * ) shape_ignore,
   ( void * ) shape_ignore_inline,
   }
};

static bool dwrite_shape(
  XentTextBackend const *backend, XentTextMeasureReq const *request, XentShapeSink *sink, XentTextMetrics *out
) {
	FluxText *tr = ( FluxText * ) backend->userdata;
	if (!tr || !request || !sink || !out) return false;

	IDWriteTextLayout *layout = dwrite_build_measure_layout(tr, request);
	if (!layout) return false;

	bool              ok       = dwrite_read_metrics(layout, request, out);
	uint32_t          len      = ( uint32_t ) strlen(request->text);
	uint32_t          wlen     = flux_utf8_to_utf16_offset(request->text, len, len);
	uint32_t         *table    = ok ? flux_utf8_utf16_byte_table(request->text, len, wlen) : NULL;

	FluxShapeRenderer renderer = {&kShapeVtbl, tr, sink, table, wlen, false};
	if (ok && table)
		ok = SUCCEEDED(IDWriteTextLayout_Draw(layout, NULL, ( IDWriteTextRenderer * ) &renderer, 0.0f, 0.0f))
		  && !renderer.failed;
	else ok = false;

	free(table);
	IDWriteTextLayout_Release(layout);
	return ok;
}

static XentTextBackend const kDWriteBackend = {
  .name     = "fluxent-dwrite",
  .measure  = dwrite_measure,
  .shape    = dwrite_shape,
  .userdata = NULL,
};

bool flux_text_register(FluxText *tr, XentCtx *ctx) {
	if (!tr || !ctx) return false;

	tr->backend          = kDWriteBackend;
	tr->backend.userdata = tr;

	return xent_text_setbackend(ctx, &tr->backend);
}
