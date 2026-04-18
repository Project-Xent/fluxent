/**
 * @file nb_validate.c
 * @brief NumberBox value parsing, validation, and stepping logic.
 */
#include "tb_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

bool nb_is_number_box(FluxTextBoxInputData *tb) {
	return xent_get_control_type(tb->ctx, tb->node) == XENT_CONTROL_NUMBER_BOX;
}

/* Coerce Value to [min, max] if validation mode is Overwrite */
void nb_coerce_value(FluxTextBoxInputData *tb) {
	double v = tb->nb_value;
	if (v != v) return;               /* NaN — leave as-is */
		if (tb->nb_validation == 0) { /* Overwrite */
			if (v > tb->nb_maximum) tb->nb_value = tb->nb_maximum;
			else if (v < tb->nb_minimum) tb->nb_value = tb->nb_minimum;
		}
}

/* Update semantic properties so snapshot/renderer can see current state */
void nb_sync_semantics(FluxTextBoxInputData *tb) {
	xent_set_semantic_value(
	  tb->ctx, tb->node, ( float ) tb->nb_value, ( float ) tb->nb_minimum, ( float ) tb->nb_maximum
	);
	xent_set_semantic_expanded(tb->ctx, tb->node, tb->nb_spin_placement == 2); /* Inline */
}

/* Format Value → text and update the buffer + TextBox display */
void nb_update_text_to_value(FluxTextBoxInputData *tb) {
	double v = tb->nb_value;
	char   tmp [64];
	int    n;
		if (v != v) { /* NaN */
			tmp [0] = '\0';
			n       = 0;
		}
		else if (v == ( double ) ( long long ) v && v >= -1e15 && v <= 1e15) {
			n = snprintf(tmp, sizeof(tmp), "%lld", ( long long ) v);
		}
		else { n = snprintf(tmp, sizeof(tmp), "%.6g", v); }
	if (n < 0) n = 0;
	if (( uint32_t ) n >= tb->buf_cap) tb_ensure_cap(tb, ( uint32_t ) n + 1);
	memcpy(tb->buffer, tmp, ( size_t ) n + 1);
	tb->buf_len              = ( uint32_t ) n;
	tb->base.content         = tb->buffer;

	tb->nb_text_updating     = true;
	/* Move cursor to end */
	tb->base.cursor_position = tb->buf_len;
	tb->base.selection_start = tb->buf_len;
	tb->base.selection_end   = tb->buf_len;
	tb_update_scroll(tb);
	tb->nb_text_updating = false;
}

/* Set Value with change detection, coercion, and event firing */
void nb_set_value(FluxTextBoxInputData *tb, double new_value) {
	if (tb->nb_value_updating) return;
	double old            = tb->nb_value;
	tb->nb_value_updating = true;

	tb->nb_value          = new_value;
	nb_coerce_value(tb);
	new_value    = tb->nb_value;

	bool changed = false;
		if (new_value != old) {
			if (!(new_value != new_value && old != old)) /* not both NaN */
				changed = true;
		}

	if (changed && tb->nb_on_value_change) tb->nb_on_value_change(tb->nb_on_value_change_ctx, new_value);

	nb_update_text_to_value(tb);
	nb_sync_semantics(tb);
	tb->nb_value_updating = false;
}

/* Parse current text → update Value (called on blur / Enter) */
void nb_validate_input(FluxTextBoxInputData *tb) {
		if (!tb->buffer || tb->buf_len == 0) {
			/* Empty text → NaN */
			double nan_val;
			*( uint64_t * ) &nan_val = 0x7ff8000000000000ull;
			nb_set_value(tb, nan_val);
			return;
		}
	/* Trim whitespace */
	char *start = tb->buffer;
	while (*start == ' ' || *start == '\t') start++;
	char  *end_ptr = NULL;
	double parsed  = strtod(start, &end_ptr);
		if (end_ptr == start || *end_ptr != '\0') {
			/* Invalid input */
			if (tb->nb_validation == 0) /* Overwrite → revert to current Value */
				nb_update_text_to_value(tb);
			return;
		}
		if (parsed == tb->nb_value) {
			/* Same value but user may have typed "1+0" or extra zeros — normalize display */
			nb_update_text_to_value(tb);
		}
		else { nb_set_value(tb, parsed); }
}

/* Step Value by `change`, with optional wrapping */
void nb_step_value(FluxTextBoxInputData *tb, double change) {
	/* Validate current text first (WinUI behavior) */
	nb_validate_input(tb);

	double v = tb->nb_value;
	if (v != v) return; /* NaN — can't step */

	v += change;

		if (tb->nb_is_wrap_enabled) {
			if (v > tb->nb_maximum) v = tb->nb_minimum;
			else if (v < tb->nb_minimum) v = tb->nb_maximum;
		}

	nb_set_value(tb, v);
}
