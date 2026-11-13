#include "controls/behavior/flux_text_select.h"

#include "render/flux_fluent.h"
#include "text/flux_clipboard.h"
#include "text/flux_utf.h"

#include <string.h>

static FluxTextStyle text_select_style(FluxTextData const *td) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = td->font_family;
	ts.font_size   = td->font_size > 0.0f ? td->font_size : 14.0f;
	ts.font_weight = td->font_weight > 0 ? td->font_weight : FLUX_FONT_REGULAR;
	ts.text_align  = td->alignment;
	ts.vert_align  = td->vertical_alignment;
	ts.word_wrap   = td->wrap;
	ts.trim        = td->trim;
	ts.max_lines   = td->max_lines;
	return ts;
}

static bool text_select_ready(FluxTextData const *td) {
	return td && td->selectable && td->text && td->ctx && td->content && td->content [0];
}

static uint32_t text_select_offset_at(FluxTextData *td, float local_x, float local_y) {
	XentRect rect = {0};
	xent_layout_rect(td->ctx, td->node, &rect);

	FluxTextStyle        ts    = text_select_style(td);
	uint32_t             len   = ( uint32_t ) strlen(td->content);
	FluxTextHitTestQuery query = {
	  {td->text, td->content, &ts, rect.w},
      local_x, local_y
    };
	return flux_utf16_to_utf8_offset(td->content, len, ( uint32_t ) flux_text_hittest(&query));
}

static void text_select_clear(FluxTextData *td) {
	td->selection_start = 0u;
	td->selection_end   = 0u;
	td->dragging        = false;
}

static void text_select_pointer_down(void *ctx, float x, float y, int click_count) {
	FluxTextData *td = ( FluxTextData * ) ctx;
	if (!text_select_ready(td)) return;

	if (click_count >= 2) {
		td->selection_start = 0u;
		td->selection_end   = ( uint32_t ) strlen(td->content);
		td->dragging        = false;
		return;
	}

	uint32_t at         = text_select_offset_at(td, x, y);
	td->drag_anchor     = at;
	td->selection_start = at;
	td->selection_end   = at;
	td->dragging        = true;
}

static void text_select_pointer_move(void *ctx, float x, float y) {
	FluxTextData *td = ( FluxTextData * ) ctx;
	if (!text_select_ready(td) || !td->dragging) return;

	td->selection_start = td->drag_anchor;
	td->selection_end   = text_select_offset_at(td, x, y);
}

static void text_select_pointer_finish(void *ctx) {
	FluxTextData *td = ( FluxTextData * ) ctx;
	if (td) td->dragging = false;
}

static void text_select_copy(FluxTextData *td) {
	uint32_t a = td->selection_start < td->selection_end ? td->selection_start : td->selection_end;
	uint32_t e = td->selection_start > td->selection_end ? td->selection_start : td->selection_end;
	if (e > a) flux_clipboard_set_utf8(td->content + a, e - a);
}

static bool text_select_key(void *ctx, FluxKey key, bool down) {
	FluxTextData *td = ( FluxTextData * ) ctx;
	if (!down || !text_select_ready(td) || !flux_key_held(FLUX_KEY_CONTROL)) return false;

	switch (key) {
	case FLUX_KEY_C : text_select_copy(td); return true;
	case FLUX_KEY_A :
		td->selection_start = 0u;
		td->selection_end   = ( uint32_t ) strlen(td->content);
		return true;
	default : return false;
	}
}

static void text_select_blur(void *ctx) {
	FluxTextData *td = ( FluxTextData * ) ctx;
	if (td) text_select_clear(td);
}

void flux_text_select_wire(FluxNodeData *nd, FluxTextData *td, bool selectable) {
	if (!nd || !td) return;
	void *ctx                        = selectable ? td : NULL;
	nd->behavior.on_pointer_down     = selectable ? text_select_pointer_down : NULL;
	nd->behavior.on_pointer_down_ctx = ctx;
	nd->behavior.on_pointer_move     = selectable ? text_select_pointer_move : NULL;
	nd->behavior.on_pointer_move_ctx = ctx;
	nd->behavior.on_cancel           = selectable ? text_select_pointer_finish : NULL;
	nd->behavior.on_cancel_ctx       = ctx;
	nd->behavior.on_key              = selectable ? text_select_key : NULL;
	nd->behavior.on_key_ctx          = ctx;
	nd->behavior.on_blur             = selectable ? text_select_blur : NULL;
	nd->behavior.on_blur_ctx         = ctx;
}
