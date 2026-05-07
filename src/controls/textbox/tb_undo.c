#include "tb_internal.h"
#include <stdlib.h>
#include <string.h>

static void tb_history_free(TbEditHistory *h) {
	free(h->deleted);
	free(h->inserted);
	memset(h, 0, sizeof(*h));
}

static void tb_pending_free(TbPendingEdit *p) {
	free(p->text);
	memset(p, 0, sizeof(*p));
}

static TbEditHistory *tb_history_at(TbEditHistory *stack, int start, int index) {
	return &stack [(start + index) % TB_UNDO_MAX];
}

static void tb_history_clear(TbEditHistory *stack, int *start, int *top) {
	for (int i = 0; i < *top; i++) tb_history_free(tb_history_at(stack, *start, i));
	*start = 0;
	*top   = 0;
}

static bool tb_copy_span(char **dst, char const *src, uint32_t len) {
	*dst = NULL;
	if (len == 0) return true;

	*dst = ( char * ) malloc(( size_t ) len);
	if (!*dst) return false;

	memcpy(*dst, src, len);
	return true;
}

static uint32_t tb_common_prefix(char const *a, uint32_t alen, char const *b, uint32_t blen) {
	uint32_t limit = alen < blen ? alen : blen;
	uint32_t pos   = 0;
	while (pos < limit && a [pos] == b [pos]) pos++;
	return pos;
}

static uint32_t tb_common_suffix(char const *a, uint32_t alen, char const *b, uint32_t blen, uint32_t prefix) {
	uint32_t suffix = 0;
	while (suffix + prefix < alen && suffix + prefix < blen) {
		if (a [alen - suffix - 1] != b [blen - suffix - 1]) break;
		suffix++;
	}
	return suffix;
}

static bool tb_history_from_pending(TbEditHistory *out, FluxTextBoxInputData const *tb) {
	memset(out, 0, sizeof(*out));
	TbPendingEdit const *p            = &tb->pending_edit;

	uint32_t             prefix       = tb_common_prefix(p->text, p->len, tb->buffer, tb->buf_len);
	uint32_t             suffix       = tb_common_suffix(p->text, p->len, tb->buffer, tb->buf_len, prefix);
	uint32_t             deleted_len  = p->len - prefix - suffix;
	uint32_t             inserted_len = tb->buf_len - prefix - suffix;
	if (deleted_len == 0 && inserted_len == 0) return false;

	out->pos              = prefix;
	out->deleted_len      = deleted_len;
	out->inserted_len     = inserted_len;
	out->before_sel_start = p->sel_start;
	out->before_sel_end   = p->sel_end;
	out->after_sel_start  = tb->base.selection_start;
	out->after_sel_end    = tb->base.selection_end;

	if (!tb_copy_span(&out->deleted, p->text + prefix, deleted_len)) return false;
	if (!tb_copy_span(&out->inserted, tb->buffer + prefix, inserted_len)) {
		tb_history_free(out);
		return false;
	}
	return true;
}

static bool tb_history_push_entry(TbEditHistory *stack, int *start, int *top, TbEditHistory *entry) {
	if (*top >= TB_UNDO_MAX) {
		tb_history_free(tb_history_at(stack, *start, 0));
		*start = (*start + 1) % TB_UNDO_MAX;
		*top   = TB_UNDO_MAX - 1;
	}
	*tb_history_at(stack, *start, *top) = *entry;
	memset(entry, 0, sizeof(*entry));
	(*top)++;
	return true;
}

static bool tb_history_pop(TbEditHistory *stack, int start, int *top, TbEditHistory *out) {
	if (*top <= 0) return false;

	TbEditHistory *src = tb_history_at(stack, start, --(*top));
	*out               = *src;
	memset(src, 0, sizeof(*src));
	return true;
}

static bool
tb_replace_span(FluxTextBoxInputData *tb, uint32_t pos, uint32_t remove_len, char const *insert, uint32_t insert_len) {
	if (pos > tb->buf_len || remove_len > tb->buf_len - pos) return false;
	if (insert_len > UINT32_MAX - (tb->buf_len - remove_len) - 1u) return false;
	if (!tb_ensure_cap(tb, tb->buf_len - remove_len + insert_len + 1u)) return false;

	memmove(tb->buffer + pos + insert_len, tb->buffer + pos + remove_len, tb->buf_len - pos - remove_len);
	if (insert_len > 0) memcpy(tb->buffer + pos, insert, insert_len);

	tb->buf_len              = tb->buf_len - remove_len + insert_len;
	tb->buffer [tb->buf_len] = '\0';
	tb->base.content         = tb->buffer;
	return true;
}

static void tb_set_restored_selection(FluxTextBoxInputData *tb, uint32_t start, uint32_t end) {
	if (start > tb->buf_len) start = tb->buf_len;
	if (end > tb->buf_len) end = tb->buf_len;
	tb->base.selection_start = start;
	tb->base.selection_end   = end;
	tb->base.cursor_position = end;
}

static void tb_history_apply(FluxTextBoxInputData *tb, TbEditHistory *entry, bool redo) {
	uint32_t remove_len  = redo ? entry->deleted_len : entry->inserted_len;
	char    *insert      = redo ? entry->inserted : entry->deleted;
	uint32_t insert_len  = redo ? entry->inserted_len : entry->deleted_len;

	tb->applying_history = true;
	if (tb_replace_span(tb, entry->pos, remove_len, insert, insert_len)) {
		if (redo) tb_set_restored_selection(tb, entry->after_sel_start, entry->after_sel_end);
		else tb_set_restored_selection(tb, entry->before_sel_start, entry->before_sel_end);
		tb_update_scroll(tb);
		tb_notify_change(tb);
	}
	tb->applying_history = false;
}

void tb_push_undo(FluxTextBoxInputData *tb) {
	if (!tb || tb->applying_history) return;

	tb_commit_pending_undo(tb);
	tb_pending_free(&tb->pending_edit);

	tb->pending_edit.text = ( char * ) malloc(( size_t ) tb->buf_len + 1u);
	if (!tb->pending_edit.text) return;

	memcpy(tb->pending_edit.text, tb->buffer, ( size_t ) tb->buf_len + 1u);
	tb->pending_edit.len       = tb->buf_len;
	tb->pending_edit.sel_start = tb->base.selection_start;
	tb->pending_edit.sel_end   = tb->base.selection_end;
	tb->pending_edit.active    = true;
}

void tb_commit_pending_undo(FluxTextBoxInputData *tb) {
	if (!tb || tb->applying_history || !tb->pending_edit.active) return;

	TbEditHistory entry;
	if (tb_history_from_pending(&entry, tb)) {
		tb_history_push_entry(tb->undo_stack, &tb->undo_start, &tb->undo_top, &entry);
		tb_history_clear(tb->redo_stack, &tb->redo_start, &tb->redo_top);
	}
	tb_pending_free(&tb->pending_edit);
}

void tb_undo(FluxTextBoxInputData *tb) {
	if (!tb) return;
	tb_commit_pending_undo(tb);
	if (tb->undo_top <= 0) return;

	TbEditHistory entry;
	if (!tb_history_pop(tb->undo_stack, tb->undo_start, &tb->undo_top, &entry)) return;
	tb_history_apply(tb, &entry, false);
	tb_history_push_entry(tb->redo_stack, &tb->redo_start, &tb->redo_top, &entry);
}

void tb_redo(FluxTextBoxInputData *tb) {
	if (!tb) return;
	tb_commit_pending_undo(tb);
	if (tb->redo_top <= 0) return;

	TbEditHistory entry;
	if (!tb_history_pop(tb->redo_stack, tb->redo_start, &tb->redo_top, &entry)) return;
	tb_history_apply(tb, &entry, true);
	tb_history_push_entry(tb->undo_stack, &tb->undo_start, &tb->undo_top, &entry);
}

void tb_free_undo_redo(FluxTextBoxInputData *tb) {
	if (!tb) return;
	tb_pending_free(&tb->pending_edit);
	tb_history_clear(tb->undo_stack, &tb->undo_start, &tb->undo_top);
	tb_history_clear(tb->redo_stack, &tb->redo_start, &tb->redo_top);
}
