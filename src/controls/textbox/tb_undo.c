/**
 * @file tb_undo.c
 * @brief Undo/redo stack management for TextBox.
 */
#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

void tb_push_undo(FluxTextBoxInputData *tb) {
		if (tb->undo_top >= TB_UNDO_MAX) {
			/* Shift down, discard oldest */
			free(tb->undo_stack [0].text);
			memmove(&tb->undo_stack [0], &tb->undo_stack [1], (TB_UNDO_MAX - 1) * sizeof(TbEditHistory));
			tb->undo_top = TB_UNDO_MAX - 1;
		}
	TbEditHistory *h = &tb->undo_stack [tb->undo_top++];
	h->text          = ( char * ) malloc(tb->buf_len + 1);
		if (h->text) {
			memcpy(h->text, tb->buffer, tb->buf_len + 1);
			h->len = tb->buf_len;
		}
	h->sel_start = tb->base.selection_start;
	h->sel_end   = tb->base.selection_end;

	/* Clear redo stack on new action */
	for (int i = 0; i < tb->redo_top; i++) free(tb->redo_stack [i].text);
	tb->redo_top = 0;
}

void tb_undo(FluxTextBoxInputData *tb) {
	if (tb->undo_top <= 0) return;

		/* Push current state to redo */
		if (tb->redo_top < TB_UNDO_MAX) {
			TbEditHistory *rh = &tb->redo_stack [tb->redo_top++];
			rh->text          = ( char * ) malloc(tb->buf_len + 1);
				if (rh->text) {
					memcpy(rh->text, tb->buffer, tb->buf_len + 1);
					rh->len = tb->buf_len;
				}
			rh->sel_start = tb->base.selection_start;
			rh->sel_end   = tb->base.selection_end;
		}

	TbEditHistory *h = &tb->undo_stack [--tb->undo_top];
		if (h->text) {
			tb_ensure_cap(tb, h->len + 1);
			memcpy(tb->buffer, h->text, h->len + 1);
			tb->buf_len      = h->len;
			tb->base.content = tb->buffer;
		}
	tb->base.selection_start = h->sel_start;
	tb->base.selection_end   = h->sel_end;
	tb->base.cursor_position = h->sel_end;
	free(h->text);
	h->text = NULL;

	tb_update_scroll(tb);
	tb_notify_change(tb);
}

void tb_redo(FluxTextBoxInputData *tb) {
	if (tb->redo_top <= 0) return;

		/* Push current state to undo */
		if (tb->undo_top < TB_UNDO_MAX) {
			TbEditHistory *uh = &tb->undo_stack [tb->undo_top++];
			uh->text          = ( char * ) malloc(tb->buf_len + 1);
				if (uh->text) {
					memcpy(uh->text, tb->buffer, tb->buf_len + 1);
					uh->len = tb->buf_len;
				}
			uh->sel_start = tb->base.selection_start;
			uh->sel_end   = tb->base.selection_end;
		}

	TbEditHistory *h = &tb->redo_stack [--tb->redo_top];
		if (h->text) {
			tb_ensure_cap(tb, h->len + 1);
			memcpy(tb->buffer, h->text, h->len + 1);
			tb->buf_len      = h->len;
			tb->base.content = tb->buffer;
		}
	tb->base.selection_start = h->sel_start;
	tb->base.selection_end   = h->sel_end;
	tb->base.cursor_position = h->sel_end;
	free(h->text);
	h->text = NULL;

	tb_update_scroll(tb);
	tb_notify_change(tb);
}

void tb_free_undo_redo(FluxTextBoxInputData *tb) {
	for (int i = 0; i < tb->undo_top; i++) free(tb->undo_stack [i].text);
	for (int i = 0; i < tb->redo_top; i++) free(tb->redo_stack [i].text);
	tb->undo_top = 0;
	tb->redo_top = 0;
}
