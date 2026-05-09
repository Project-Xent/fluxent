#include "flux_control_cursor.h"

#include "fluxent/flux_component_data.h"
#include "fluxent/flux_window.h"

static int text_input_cursor(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(store, node);
	if (!nd || nd->hover_local_x < 0.0f || !nd->state.focused) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);

	FluxTextBoxData *td = ( FluxTextBoxData * ) nd->component_data;
	if (!td || !td->content || !td->content [0] || td->readonly) return FLUX_CURSOR_IBEAM;

	return nd->hover_local_x >= rect.width - 30.0f ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
}

static int password_cursor(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(store, node);
	if (!nd || !nd->state.focused) return FLUX_CURSOR_IBEAM;

	FluxPasswordBoxData *pd = ( FluxPasswordBoxData * ) nd->component_data;
	if (!pd || !pd->content || !pd->content [0]) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);
	return nd->hover_local_x >= rect.width - 30.0f ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
}

static int number_cursor(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(store, node);
	if (!nd || nd->hover_local_x < 0.0f) return FLUX_CURSOR_IBEAM;

	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);

	bool  spin   = xent_get_semantic_expanded(ctx, node);
	float spin_w = spin ? 76.0f : 0.0f;
	if (spin && nd->hover_local_x >= rect.width - spin_w) return FLUX_CURSOR_ARROW;

	if (!nd->state.focused) return FLUX_CURSOR_IBEAM;

	FluxTextBoxData *td = ( FluxTextBoxData * ) nd->component_data;
	if (!td || !td->content || !td->content [0] || td->readonly) return FLUX_CURSOR_IBEAM;

	float del_start = rect.width - 40.0f - spin_w;
	return nd->hover_local_x >= del_start && nd->hover_local_x < del_start + 40.0f ? FLUX_CURSOR_ARROW
	                                                                               : FLUX_CURSOR_IBEAM;
}

static int default_cursor_for_type(XentControlType type) {
	static int const cursors [XENT_CONTROL_CUSTOM + 1] = {
	  [XENT_CONTROL_BUTTON]        = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_TOGGLE_BUTTON] = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_CHECKBOX]      = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_RADIO]         = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_SWITCH]        = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_SLIDER]        = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_HYPERLINK]     = FLUX_CURSOR_HAND,
	  [XENT_CONTROL_REPEAT_BUTTON] = FLUX_CURSOR_HAND,
	};
	if (type < 0 || type > XENT_CONTROL_CUSTOM) return FLUX_CURSOR_ARROW;
	return cursors [type] ? cursors [type] : FLUX_CURSOR_ARROW;
}

int flux_control_cursor_for_node(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	if (!ctx || !store || node == XENT_NODE_INVALID) return FLUX_CURSOR_ARROW;

	XentControlType type = xent_get_control_type(ctx, node);
	switch (type) {
	case XENT_CONTROL_TEXT_INPUT   : return text_input_cursor(ctx, store, node);
	case XENT_CONTROL_PASSWORD_BOX : return password_cursor(ctx, store, node);
	case XENT_CONTROL_NUMBER_BOX   : return number_cursor(ctx, store, node);
	default                        : return default_cursor_for_type(type);
	}
}
