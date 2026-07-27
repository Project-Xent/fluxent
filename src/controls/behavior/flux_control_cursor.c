#include "flux_control_cursor.h"
#include "store/flux_node_data.h"

#include "controls/textbox/tb_metrics.h"
#include "controls/data/flux_component_data.h"
#include "fluxent/flux_window.h"

static int text_input_cursor(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->hover_local_x < 0.0f || !nd->state.focused) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);

	FluxTextBoxData *td = ( FluxTextBoxData * ) nd->component_data;
	if (!td || !td->content || !td->content [0] || td->readonly) return FLUX_CURSOR_IBEAM;

	return nd->hover_local_x >= rect.w - FLUX_TEXTBOX_ACTION_BUTTON_W ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
}

static int password_cursor(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || !nd->state.focused) return FLUX_CURSOR_IBEAM;

	FluxPasswordBoxData *pd = ( FluxPasswordBoxData * ) nd->component_data;
	if (!pd || !pd->content || !pd->content [0]) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);
	return nd->hover_local_x >= rect.w - FLUX_PASSWORD_REVEAL_BTN_W ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
}

static int number_cursor(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->hover_local_x < 0.0f) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);

	bool  spin   = xent_sem_expanded(ctx, node);
	float spin_w = spin ? FLUX_NUMBER_BOX_SPIN_W : 0.0f;
	if (spin && nd->hover_local_x >= rect.w - spin_w) return FLUX_CURSOR_ARROW;

	if (!nd->state.focused) return FLUX_CURSOR_IBEAM;

	FluxTextBoxData *td = ( FluxTextBoxData * ) nd->component_data;
	if (!td || !td->content || !td->content [0] || td->readonly) return FLUX_CURSOR_IBEAM;

	float del_start = rect.w - FLUX_NUMBER_BOX_DELETE_BTN_W - spin_w;
	return nd->hover_local_x >= del_start && nd->hover_local_x < del_start + FLUX_NUMBER_BOX_DELETE_BTN_W
	       ? FLUX_CURSOR_ARROW
	       : FLUX_CURSOR_IBEAM;
}

static int default_cursor_for_type(FluxControlType type) {
	static int const cursors [XTK_CTRL_TYPE_COUNT] = {
	  [FLUX_CONTROL_BUTTON]        = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_TOGGLE_BUTTON] = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_CHECKBOX]      = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_RADIO]         = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_SWITCH]        = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_SLIDER]        = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_HYPERLINK]     = FLUX_CURSOR_HAND,
	  [FLUX_CONTROL_REPEAT_BUTTON] = FLUX_CURSOR_HAND,
	};
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return FLUX_CURSOR_ARROW;
	return cursors [type] ? cursors [type] : FLUX_CURSOR_ARROW;
}

int flux_control_cursor_for(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	if (!ctx || !store || node == XENT_NODE_INVALID) return FLUX_CURSOR_ARROW;

	FluxControlType type = flux_node_type(store, node);
	switch (type) {
	case FLUX_CONTROL_TEXT_INPUT   : return text_input_cursor(ctx, store, node);
	case FLUX_CONTROL_PASSWORD_BOX : return password_cursor(ctx, store, node);
	case FLUX_CONTROL_NUMBER_BOX   : return number_cursor(ctx, store, node);
	default                        : return default_cursor_for_type(type);
	}
}
