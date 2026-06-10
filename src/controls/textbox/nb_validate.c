#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "tb_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cwinrt/cast.h>
#include <cwinrt/hstring.h>
#include <cwinrt/Windows.Globalization.NumberFormatting.h>

bool                           nb_is_number_box(FluxTextBoxInputData *tb) { return tb->nb != NULL; }

/* Process-wide locale formatter shared by every number box (single UI thread).
 * Deliberate global: one DecimalFormatter, created on first use, lives until exit. */
static WGLNU_DecimalFormatter *g_nb_formatter;
static bool                    g_nb_formatter_ready;

static void                    nb_formatter_attach_rounder(WGLNU_DecimalFormatter *f) {
	WGLNU_IncrementNumberRounder *rounder = NULL;
	if (FAILED(wglnu_increment_number_rounder_new(&rounder)) || !rounder) return;

	wglnu_increment_number_rounder_put__increment(rounder, 0.000001);
	wglnu_increment_number_rounder_put__rounding_algorithm(rounder, WGLNU_RoundingAlgorithm_RoundHalfAwayFromZero);

	WGLNU_INumberRounder *as_rounder = NULL;
	if (SUCCEEDED(cwinrt_query(rounder, &CWINRT_IID_WGLNU_INumberRounder, ( void ** ) &as_rounder))) {
		wglnu_decimal_formatter_put__number_rounder(f, as_rounder);
		(( IUnknown * ) as_rounder)->lpVtbl->Release(( IUnknown * ) as_rounder);
	}
	(( IUnknown * ) rounder)->lpVtbl->Release(( IUnknown * ) rounder);
}

static WGLNU_DecimalFormatter *nb_locale_formatter(void) {
	if (g_nb_formatter_ready) return g_nb_formatter;
	g_nb_formatter_ready = true;

	if (FAILED(wglnu_decimal_formatter_new(&g_nb_formatter))) {
		g_nb_formatter = NULL;
		return NULL;
	}
	wglnu_decimal_formatter_put__is_grouped(g_nb_formatter, TRUE);
	wglnu_decimal_formatter_put__fraction_digits(g_nb_formatter, 0);
	nb_formatter_attach_rounder(g_nb_formatter);
	return g_nb_formatter;
}

typedef struct NbFormatResult {
	char     stack [64];
	char    *text;
	uint32_t len;
} NbFormatResult;

static bool nb_format_stack(NbFormatResult *fmt, double v, char const *pattern) {
	int n = snprintf(fmt->stack, sizeof(fmt->stack), pattern, v);
	if (n < 0) return false;
	if (( size_t ) n < sizeof(fmt->stack)) {
		fmt->text = fmt->stack;
		fmt->len  = ( uint32_t ) n;
		return true;
	}

	fmt->text = ( char * ) malloc(( size_t ) n + 1u);
	if (!fmt->text) return false;
	n = snprintf(fmt->text, ( size_t ) n + 1u, pattern, v);
	if (n < 0) {
		free(fmt->text);
		memset(fmt, 0, sizeof(*fmt));
		return false;
	}
	fmt->len = ( uint32_t ) n;
	return true;
}

static bool nb_format_int(NbFormatResult *fmt, long long v) {
	int n = snprintf(fmt->stack, sizeof(fmt->stack), "%lld", v);
	if (n < 0) return false;
	if (( size_t ) n < sizeof(fmt->stack)) {
		fmt->text = fmt->stack;
		fmt->len  = ( uint32_t ) n;
		return true;
	}

	fmt->text = ( char * ) malloc(( size_t ) n + 1u);
	if (!fmt->text) return false;
	n = snprintf(fmt->text, ( size_t ) n + 1u, "%lld", v);
	if (n < 0) {
		free(fmt->text);
		memset(fmt, 0, sizeof(*fmt));
		return false;
	}
	fmt->len = ( uint32_t ) n;
	return true;
}

static bool nb_value_is_integral(double v) { return v >= -1e15 && v <= 1e15 && v == ( double ) ( long long ) v; }

static bool nb_format_with_locale(NbFormatResult *fmt, double v) {
	WGLNU_DecimalFormatter *f = nb_locale_formatter();
	if (!f) return false;

	cwinrt_hstring hs = NULL;
	HRESULT        hr = nb_value_is_integral(v) ? wglnu_decimal_formatter_format_int(f, ( int64_t ) v, &hs)
	                                            : wglnu_decimal_formatter_format_double(f, v, &hs);
	if (FAILED(hr)) return false;

	int n = cwinrt_hstring_to_utf8(hs, fmt->stack, sizeof(fmt->stack));
	cwinrt_hstring_free(hs);
	if (n < 0) return false;

	fmt->text = fmt->stack;
	fmt->len  = ( uint32_t ) n;
	return true;
}

static bool nb_format_value(NbFormatResult *fmt, double v) {
	memset(fmt, 0, sizeof(*fmt));
	if (isnan(v)) {
		fmt->text = fmt->stack;
		fmt->len  = 0;
		return true;
	}
	if (nb_format_with_locale(fmt, v)) return true;
	if (nb_value_is_integral(v)) return nb_format_int(fmt, ( long long ) v);
	return nb_format_stack(fmt, v, "%.6g");
}

static void nb_format_free(NbFormatResult *fmt) {
	if (fmt->text != fmt->stack) free(fmt->text);
}

void nb_coerce_value(FluxTextBoxInputData *tb) {
	double v = tb->nb->value;
	if (isnan(v) || tb->nb->validation != 0) return;
	if (v > tb->nb->maximum) tb->nb->value = tb->nb->maximum;
	else if (v < tb->nb->minimum) tb->nb->value = tb->nb->minimum;
}

void nb_sync_semantics(FluxTextBoxInputData *tb) {
	xent_set_semantic_value(
	  tb->ctx, tb->node, ( float ) tb->nb->value, ( float ) tb->nb->minimum, ( float ) tb->nb->maximum
	);
	xent_set_semantic_expanded(tb->ctx, tb->node, tb->nb->spin_placement == 2);
}

void nb_update_text_to_value(FluxTextBoxInputData *tb) {
	NbFormatResult fmt;
	if (!nb_format_value(&fmt, tb->nb->value)) return;

	bool ok = tb_replace_text(tb, fmt.text ? fmt.text : "");
	nb_format_free(&fmt);
	if (!ok) return;

	tb->nb->text_updating = true;
	tb_update_scroll(tb);
	tb->nb->text_updating = false;
}

void nb_set_value(FluxTextBoxInputData *tb, double new_value) {
	if (tb->nb->value_updating) return;
	double old             = tb->nb->value;
	tb->nb->value_updating = true;

	tb->nb->value          = new_value;
	nb_coerce_value(tb);
	new_value    = tb->nb->value;

	bool changed = (new_value != old) && !(isnan(new_value) && isnan(old));
	if (changed && tb->nb->on_value_change) tb->nb->on_value_change(tb->nb->on_value_change_ctx, new_value);

	nb_update_text_to_value(tb);
	nb_sync_semantics(tb);
	tb->nb->value_updating = false;
}

/* INumberParser.ParseDouble yields a Windows.Foundation.IReference<double>*, which
 * cwinrt surfaces untyped; its Value() accessor is vtable slot 6 (after IInspectable's
 * six entries). Reads the value and releases the reference. */
static bool nb_unwrap_double(void *ref, double *out) {
	if (!ref) return false;
	typedef HRESULT(__stdcall * value_fn)(void *self, double *out);
	value_fn fn = ( value_fn ) (( void ** ) (( IUnknown * ) ref)->lpVtbl) [6];
	HRESULT  hr = fn(ref, out);
	(( IUnknown * ) ref)->lpVtbl->Release(( IUnknown * ) ref);
	return SUCCEEDED(hr);
}

static bool nb_parse_strtod(char const *s, double *out) {
	char  *end = NULL;
	double v   = strtod(s, &end);
	if (end == s) return false;
	while (*end == ' ' || *end == '\t') end++;
	if (*end != '\0') return false;
	*out = v;
	return true;
}

static bool nb_parse(char const *s, double *out) {
	WGLNU_DecimalFormatter *f = nb_locale_formatter();
	cwinrt_hstring          hs;
	if (f && SUCCEEDED(cwinrt_hstring_from_utf8(s, &hs))) {
		double *ref = NULL;
		HRESULT hr  = wglnu_decimal_formatter_parse_double(f, hs, &ref);
		cwinrt_hstring_free(hs);
		if (SUCCEEDED(hr) && ref) return nb_unwrap_double(ref, out);
	}
	return nb_parse_strtod(s, out);
}

void nb_validate_input(FluxTextBoxInputData *tb) {
	if (!tb->buffer || tb->buf_len == 0) {
		nb_set_value(tb, NAN);
		return;
	}

	double parsed;
	if (!nb_parse(tb_sync_content(tb), &parsed)) {
		if (tb->nb->validation == 0) nb_update_text_to_value(tb);
		return;
	}
	if (parsed == tb->nb->value) nb_update_text_to_value(tb);
	else nb_set_value(tb, parsed);
}

/* Wrap a stepped value around the [min, max] range (WinUI NumberBox wrap behavior). */
static double nb_wrap(double v, double min, double max) {
	if (v > max) return min;
	if (v < min) return max;
	return v;
}

void nb_step_value(FluxTextBoxInputData *tb, double change) {
	nb_validate_input(tb);

	double v = tb->nb->value;
	if (isnan(v)) return;

	v += change;
	if (tb->nb->is_wrap_enabled) v = nb_wrap(v, tb->nb->minimum, tb->nb->maximum);

	nb_set_value(tb, v);
}
