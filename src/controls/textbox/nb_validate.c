#include "tb_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

bool nb_is_number_box(FluxTextBoxInputData *tb) { return tb->nb != NULL; }

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

static bool nb_format_value(NbFormatResult *fmt, double v) {
	memset(fmt, 0, sizeof(*fmt));
	if (isnan(v)) {
		fmt->text = fmt->stack;
		fmt->len  = 0;
		return true;
	}
	if (v >= -1e15 && v <= 1e15 && v == ( double ) ( long long ) v) return nb_format_int(fmt, ( long long ) v);
	return nb_format_stack(fmt, v, "%.6g");
}

static void nb_format_free(NbFormatResult *fmt) {
	if (fmt->text != fmt->stack) free(fmt->text);
}

void nb_coerce_value(FluxTextBoxInputData *tb) {
	double v = tb->nb->value;
	if (isnan(v)) return;
	if (tb->nb->validation == 0) {
		if (v > tb->nb->maximum) tb->nb->value = tb->nb->maximum;
		else if (v < tb->nb->minimum) tb->nb->value = tb->nb->minimum;
	}
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
	if (fmt.len >= tb->buf_cap && !tb_ensure_cap(tb, fmt.len + 1u)) {
		nb_format_free(&fmt);
		return;
	}
	memcpy(tb->buffer, fmt.text, ( size_t ) fmt.len);
	tb->buffer [fmt.len] = '\0';
	tb->buf_len          = fmt.len;
	tb->base.content     = tb->buffer;
	nb_format_free(&fmt);

	tb->nb->text_updating    = true;
	tb->base.cursor_position = tb->buf_len;
	tb->base.selection_start = tb->buf_len;
	tb->base.selection_end   = tb->buf_len;
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

void nb_validate_input(FluxTextBoxInputData *tb) {
	if (!tb->buffer || tb->buf_len == 0) {
		nb_set_value(tb, NAN);
		return;
	}
	char *start = tb->buffer;
	while (*start == ' ' || *start == '\t') start++;
	char  *end_ptr = NULL;
	double parsed  = strtod(start, &end_ptr);
	if (end_ptr == start || *end_ptr != '\0') {
		if (tb->nb->validation == 0) nb_update_text_to_value(tb);
		return;
	}
	if (parsed == tb->nb->value) nb_update_text_to_value(tb);
	else nb_set_value(tb, parsed);
}

void nb_step_value(FluxTextBoxInputData *tb, double change) {
	nb_validate_input(tb);

	double v = tb->nb->value;
	if (isnan(v)) return;

	v += change;

	if (tb->nb->is_wrap_enabled) {
		if (v > tb->nb->maximum) v = tb->nb->minimum;
		else if (v < tb->nb->minimum) v = tb->nb->maximum;
	}

	nb_set_value(tb, v);
}
