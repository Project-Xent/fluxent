#include "tb_internal.h"
#include "bridge/flux_internal.h"
#include "controls/flux_module.h"
#include "render/flux_fluent.h"

static FluxTextBoxInputData *tb_uia_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_TEXT_INPUT ? ( FluxTextBoxInputData * ) nd->component_data : NULL;
}

bool flux_textbox_uia_view(FluxStore *store, XentNodeId node, FluxUiaTextView *out) {
	FluxTextBoxInputData *tb   = tb_uia_data(store, node);
	XentRect              rect = {0};
	if (!tb) return false;
	xent_layout_rect(tb->ctx, node, &rect);
	float visible = tb_text_max_w(tb, FLUX_CONTROL_TEXT_INPUT);
	*out          = (FluxUiaTextView) {
	  .renderer   = tb->app ? flux_app_text(tb->app) : NULL,
	  .text       = tb_sync_content(tb),
	  .style      = tb_make_style(tb),
	  .layout_w   = tb_layout_width(tb, visible),
	  .origin_x   = FLUX_TEXTBOX_PAD_L - tb->base.scroll_offset_x,
	  .origin_y   = FLUX_TEXTBOX_PAD_T - tb->base.scroll_offset_y,
	  .clip       = {FLUX_TEXTBOX_PAD_L, FLUX_TEXTBOX_PAD_T, visible, tb_visible_text_height(tb, &rect)},
	  .sel_start  = tb->base.selection_start,
	  .sel_end    = tb->base.selection_end,
	  .selectable = true,
	  .readonly   = tb->base.readonly,
	  .paragraphs = true,
	};
	out->style.vert_align = FLUX_TEXT_TOP;
	out->style.color      = tb->base.text_color;
	return true;
}

void flux_textbox_uia_select(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end) {
	FluxTextBoxInputData *tb = tb_uia_data(store, node);
	if (!tb) return;
	tb->caret_column_held    = false;
	tb->base.selection_start = start;
	tb->base.selection_end   = end;
	tb->base.cursor_position = end;
	tb_update_scroll(tb);
}

void flux_textbox_uia_reveal(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end) {
	FluxTextBoxInputData *tb = tb_uia_data(store, node);
	if (!tb) return;
	uint32_t cursor          = tb->base.cursor_position;
	tb->base.cursor_position = end;
	tb_update_scroll(tb);
	tb->base.cursor_position = start;
	tb_update_scroll(tb);
	tb->base.cursor_position = cursor;
}
