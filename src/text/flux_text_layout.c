#include "text/flux_text_internal.h"

#include <stdlib.h>
#include <string.h>

DWRITE_WORD_WRAPPING policy_to_dwrite_wrapping(XentLineBreakPolicy policy, XentMeasureMode mode) {
	if (mode == XENT_MEASURE_UNDEFINED) return DWRITE_WORD_WRAPPING_NO_WRAP;
	switch (policy) {
	case XENT_LINEBREAK_NOWRAP   : return DWRITE_WORD_WRAPPING_NO_WRAP;
	case XENT_LINEBREAK_WORDWRAP : return DWRITE_WORD_WRAPPING_WRAP;
	case XENT_LINEBREAK_CHARWRAP : return DWRITE_WORD_WRAPPING_CHARACTER;
	default                      : return DWRITE_WORD_WRAPPING_NO_WRAP;
	}
}

float constraint_for_mode(float width_constraint, XentMeasureMode mode) {
	switch (mode) {
	case XENT_MEASURE_AT_MOST :
	case XENT_MEASURE_EXACTLY : return width_constraint;
	default                   : return 100000.0f;
	}
}

DWRITE_TEXT_ALIGNMENT to_dw_align(FluxTextAlign a) {
	switch (a) {
	case FLUX_TEXT_CENTER  : return DWRITE_TEXT_ALIGNMENT_CENTER;
	case FLUX_TEXT_RIGHT   : return DWRITE_TEXT_ALIGNMENT_TRAILING;
	case FLUX_TEXT_JUSTIFY : return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
	default                : return DWRITE_TEXT_ALIGNMENT_LEADING;
	}
}

DWRITE_PARAGRAPH_ALIGNMENT to_dw_para(FluxTextVAlign a) {
	switch (a) {
	case FLUX_TEXT_VCENTER : return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	case FLUX_TEXT_BOTTOM  : return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
	default                : return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
	}
}

static DWRITE_FONT_STYLE to_dw_slant(XentFontSlant slant) {
	switch (slant) {
	case XENT_FONT_SLANT_ITALIC  : return DWRITE_FONT_STYLE_ITALIC;
	case XENT_FONT_SLANT_OBLIQUE : return DWRITE_FONT_STYLE_OBLIQUE;
	default                      : return DWRITE_FONT_STYLE_NORMAL;
	}
}

static DWRITE_TRIMMING_GRANULARITY to_dw_granularity(XentTextTrim trim) {
	switch (trim) {
	case XENT_TEXT_TRIM_CHAR          :
	case XENT_TEXT_TRIM_CHAR_ELLIPSIS : return DWRITE_TRIMMING_GRANULARITY_CHARACTER;
	case XENT_TEXT_TRIM_WORD          :
	case XENT_TEXT_TRIM_WORD_ELLIPSIS : return DWRITE_TRIMMING_GRANULARITY_WORD;
	default                           : return DWRITE_TRIMMING_GRANULARITY_NONE;
	}
}

static bool trim_shows_ellipsis(XentTextTrim trim) {
	return trim == XENT_TEXT_TRIM_CHAR_ELLIPSIS || trim == XENT_TEXT_TRIM_WORD_ELLIPSIS;
}

static DWRITE_TEXT_RANGE run_to_range(char const *utf8, XentTextRun const *run) {
	uint32_t          len   = ( uint32_t ) strlen(utf8);
	UINT32            start = flux_utf8_to_utf16_offset(utf8, len, run->offset);
	UINT32            end   = flux_utf8_to_utf16_offset(utf8, len, run->offset + run->length);
	DWRITE_TEXT_RANGE range = {start, end > start ? end - start : 0u};
	return range;
}

static void apply_run_style(IDWriteTextLayout *layout, char const *utf8, XentTextRun const *run) {
	DWRITE_TEXT_RANGE range = run_to_range(utf8, run);
	if (range.length == 0u) return;

	IDWriteTextLayout_SetFontSize(layout, run->font_size, range);
	IDWriteTextLayout_SetFontWeight(layout, ( DWRITE_FONT_WEIGHT ) (run->font_weight ? run->font_weight : 400), range);
	IDWriteTextLayout_SetFontStretch(
	  layout, ( DWRITE_FONT_STRETCH ) (run->font_stretch ? run->font_stretch : 5), range
	);
	IDWriteTextLayout_SetFontStyle(layout, to_dw_slant(run->slant), range);

	wchar_t family [FLUX_FAMILY_BUF];
	if (run->font_family && run->font_family [0] && flux_utf8_to_wchar(run->font_family, family, FLUX_FAMILY_BUF) > 0)
		IDWriteTextLayout_SetFontFamilyName(layout, family, range);
}

static void apply_trimming(FluxText *tr, IDWriteTextLayout *layout, XentTextTrim trim) {
	if (trim == XENT_TEXT_TRIM_NONE) return;

	DWRITE_TRIMMING      options = {to_dw_granularity(trim), 0u, 0u};
	IDWriteInlineObject *sign    = NULL;
	if (trim_shows_ellipsis(trim))
		( void ) IDWriteFactory_CreateEllipsisTrimmingSign(tr->factory, ( IDWriteTextFormat * ) layout, &sign);

	IDWriteTextLayout_SetTrimming(layout, &options, sign);
	if (sign) IDWriteInlineObject_Release(sign);
}

bool read_line_metrics(IDWriteTextLayout *layout, TextLineMetrics *out) {
	out->lines    = out->stack;
	out->count    = 0u;

	UINT32 actual = 0u;
	IDWriteTextLayout_GetLineMetrics(layout, NULL, 0u, &actual);
	if (actual == 0u) return false;

	if (actual > ( UINT32 ) (sizeof(out->stack) / sizeof(out->stack [0]))) {
		out->lines = ( DWRITE_LINE_METRICS * ) malloc(sizeof(*out->lines) * ( size_t ) actual);
		if (!out->lines) {
			out->lines = out->stack;
			return false;
		}
	}

	if (FAILED(IDWriteTextLayout_GetLineMetrics(layout, out->lines, actual, &actual))) {
		if (out->lines != out->stack) free(out->lines);
		out->lines = out->stack;
		return false;
	}
	out->count = actual;
	return true;
}

void free_line_metrics(TextLineMetrics *m) {
	if (m->lines != m->stack) free(m->lines);
	m->lines = m->stack;
	m->count = 0u;
}

float kept_line_height(TextLineMetrics const *m, uint32_t max_lines) {
	uint32_t kept   = (max_lines && max_lines < m->count) ? max_lines : m->count;
	float    height = 0.0f;
	for (uint32_t i = 0u; i < kept; ++i) height += m->lines [i].height;
	return height;
}

static void apply_max_lines(IDWriteTextLayout *layout, uint32_t max_lines) {
	if (max_lines == 0u) return;

	TextLineMetrics m;
	if (!read_line_metrics(layout, &m)) return;
	if (m.count > max_lines) IDWriteTextLayout_SetMaxHeight(layout, kept_line_height(&m, max_lines));
	free_line_metrics(&m);
}

void text_configure_layout(FluxText *tr, IDWriteTextLayout *layout, char const *utf8, FluxTextStyle const *style) {
	for (uint32_t i = 0u; i < style->run_count; ++i) apply_run_style(layout, utf8, &style->runs [i]);
	apply_trimming(tr, layout, style->trim);
	apply_max_lines(layout, style->max_lines);
}
