#include "flux_input_internal.h"

#include <stdlib.h>

static bool input_node_is_visible_focusable(XentContext *ctx, XentNodeId node) {
	if (!xent_get_focusable(ctx, node) || !xent_get_semantic_enabled(ctx, node)) return false;

	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);
	return rect.width > 0.0f && rect.height > 0.0f;
}

static void collect_focusable(XentContext *ctx, XentNodeId node, XentNodeId *out, uint32_t *count, uint32_t max_count) {
	if (node == XENT_NODE_INVALID || *count >= max_count) return;

	if (input_node_is_visible_focusable(ctx, node)) {
		out [*count] = node;
		(*count)++;
	}

	XentNodeId child = xent_get_first_child(ctx, node);
	while (child != XENT_NODE_INVALID && *count < max_count) {
		collect_focusable(ctx, child, out, count, max_count);
		child = xent_get_next_sibling(ctx, child);
	}
}

static XentContext *s_sort_ctx = NULL;

static int          compare_tab_index(void const *a, void const *b) {
	XentNodeId na = *( XentNodeId const * ) a;
	XentNodeId nb = *( XentNodeId const * ) b;
	int32_t    ta = xent_get_tab_index(s_sort_ctx, na);
	int32_t    tb = xent_get_tab_index(s_sort_ctx, nb);

	int32_t    ea = (ta == 0) ? 0x7fffffff : ta;
	int32_t    eb = (tb == 0) ? 0x7fffffff : tb;

	if (ea != eb) return (ea < eb) ? -1 : 1;
	return (na < nb) ? -1 : (na > nb) ? 1 : 0;
}

static uint32_t
input_collect_tab_order(XentContext *ctx, XentNodeId const *focusable, uint32_t count, XentNodeId *tab_order) {
	uint32_t tab_count = 0;
	for (uint32_t i = 0; i < count; i++) {
		int32_t ti = xent_get_tab_index(ctx, focusable [i]);
		if (ti >= 0) tab_order [tab_count++] = focusable [i];
	}
	return tab_count;
}

static int input_find_node_index(XentNodeId const *nodes, uint32_t count, XentNodeId target) {
	for (uint32_t i = 0; i < count; i++)
		if (nodes [i] == target) return ( int ) i;
	return -1;
}

static int input_next_tab_index(int current_idx, uint32_t tab_count, bool shift) {
	if (current_idx < 0) return shift ? ( int ) tab_count - 1 : 0;
	if (shift) return (current_idx - 1 + ( int ) tab_count) % ( int ) tab_count;
	return (current_idx + 1) % ( int ) tab_count;
}

static void input_focus_node(FluxInput *input, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, node);
	if (!nd) return;

	nd->state.focused = 1;
	if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
}

static void input_move_focus(FluxInput *input, XentNodeId node) {
	input_blur_focused(input);
	input->focused = node;
	input_focus_node(input, node);
}

static uint32_t
input_collect_focusable_siblings(XentContext *ctx, XentNodeId parent, XentNodeId *siblings, uint32_t max_count) {
	uint32_t   count = 0;
	XentNodeId child = xent_get_first_child(ctx, parent);
	while (child != XENT_NODE_INVALID && count < max_count) {
		if (xent_get_focusable(ctx, child) && xent_get_semantic_enabled(ctx, child)) siblings [count++] = child;
		child = xent_get_next_sibling(ctx, child);
	}
	return count;
}

static int input_arrow_delta(int direction) {
	if (direction == VK_LEFT || direction == VK_UP) return -1;
	if (direction == VK_RIGHT || direction == VK_DOWN) return 1;
	return 0;
}

void flux_input_tab(FluxInput *input, XentNodeId root, bool shift) {
	if (!input || root == XENT_NODE_INVALID) return;

	XentNodeId focusable [512];
	uint32_t   count = 0;
	collect_focusable(input->ctx, root, focusable, &count, 512);

	if (count == 0) return;

	XentNodeId tab_order [512];
	uint32_t   tab_count = input_collect_tab_order(input->ctx, focusable, count, tab_order);
	if (tab_count == 0) return;

	s_sort_ctx = input->ctx;
	qsort(tab_order, tab_count, sizeof(XentNodeId), compare_tab_index);
	s_sort_ctx             = NULL;

	int        current_idx = input_find_node_index(tab_order, tab_count, input->focused);
	int        next_idx    = input_next_tab_index(current_idx, tab_count, shift);
	XentNodeId next_node   = tab_order [next_idx];
	input_move_focus(input, next_node);
}

void flux_input_arrow(FluxInput *input, XentNodeId root, int direction) {
	if (!input || root == XENT_NODE_INVALID) return;
	if (input->focused == XENT_NODE_INVALID) return;

	XentNodeId parent = xent_get_parent(input->ctx, input->focused);
	if (parent == XENT_NODE_INVALID) return;

	XentNodeId siblings [128];
	uint32_t   sib_count = input_collect_focusable_siblings(input->ctx, parent, siblings, 128);
	if (sib_count <= 1) return;

	int cur = input_find_node_index(siblings, sib_count, input->focused);
	if (cur < 0) return;

	int delta = input_arrow_delta(direction);
	if (delta == 0) return;

	int        next      = (cur + delta + ( int ) sib_count) % ( int ) sib_count;
	XentNodeId next_node = siblings [next];
	input_move_focus(input, next_node);
}

void flux_input_activate(FluxInput *input) {
	if (!input || input->focused == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_click) nd->behavior.on_click(nd->behavior.on_click_ctx);
}

void flux_input_escape(FluxInput *input) {
	if (!input) return;

	input_blur_focused(input);
	input->focused = XENT_NODE_INVALID;
}

void flux_input_set_focus(FluxInput *input, XentNodeId node) {
	if (!input) return;

	if (input->focused != node) input_blur_focused(input);
	input->focused = node;
	input_focus_node(input, node);
}
