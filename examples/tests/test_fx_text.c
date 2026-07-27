#include <math.h>
#include <stdio.h>

#include "fluxent/flux_text.h"

#define CHECK(expr)                                                         \
	do {                                                                    \
		if (!(expr)) {                                                      \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			return 1;                                                       \
		}                                                                   \
	}                                                                       \
	while (0)

static bool measure(
  XentCtx *ctx, char const *text, XentLineBreakPolicy policy, XentMeasureMode mode, float width, XentTextMetrics *out
) {
	XentTextMeasureReq request = {text, 14.0f, 400u, width, policy, mode};
	return xent_text_measure(ctx, &request, out);
}

int main(void) {
	XentCtx  *ctx  = xent_ctx_create(NULL);
	FluxText *text = flux_text_create();
	CHECK(ctx != NULL);
	CHECK(text != NULL);
	CHECK(flux_text_register(text, ctx));

	XentTextMetrics full = {0};
	CHECK(measure(ctx, "alpha beta gamma", XENT_LINEBREAK_NOWRAP, XENT_MEASURE_UNDEFINED, INFINITY, &full));
	CHECK(full.width > 0.0f);
	CHECK(full.line_count == 1u);

	XentTextMetrics word_min = {0};
	CHECK(measure(ctx, "alpha beta gamma", XENT_LINEBREAK_WORDWRAP, XENT_MEASURE_MIN_CONTENT, INFINITY, &word_min));
	CHECK(word_min.width > 0.0f);
	CHECK(word_min.width < full.width);
	CHECK(word_min.line_count >= 2u);

	XentTextMetrics char_min = {0};
	CHECK(measure(ctx, "alpha beta gamma", XENT_LINEBREAK_CHARWRAP, XENT_MEASURE_MIN_CONTENT, INFINITY, &char_min));
	CHECK(char_min.width > 0.0f);
	CHECK(char_min.width <= word_min.width);

	XentTextMetrics exact = {0};
	CHECK(measure(ctx, "alpha", XENT_LINEBREAK_NOWRAP, XENT_MEASURE_EXACTLY, 73.0f, &exact));
	CHECK(fabsf(exact.width - 73.0f) < 0.001f);

	XentTextMetrics empty = {0};
	CHECK(measure(ctx, "", XENT_LINEBREAK_NOWRAP, XENT_MEASURE_UNDEFINED, INFINITY, &empty));
	CHECK(empty.width == 0.0f);

	flux_text_destroy(text);
	xent_ctx_destroy(ctx);
	return 0;
}
