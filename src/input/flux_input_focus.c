#include "flux_input_internal.h"
#include "bridge/flux_internal.h"

#include <fluxent/flux_node_store.h>
#include "store/flux_node_data.h"
#include <stdlib.h>

static bool input_node_is_visible_focusable(XentCtx *ctx, XentNodeId node) {
	if (!xent_get_focusable(ctx, node) || !xent_sem_enabled(ctx, node)) return false;

	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);
	return rect.w > 0.0f && rect.h > 0.0f;
}

static void collect_focusable(XentCtx *ctx, XentNodeId node, XentNodeId *out, uint32_t *count, uint32_t max_count) {
	if (node == XENT_NODE_INVALID || *count >= max_count) return;

	if (input_node_is_visible_focusable(ctx, node)) {
		out [*count] = node;
		(*count)++;
	}

	XentNodeId child = xent_node_first(ctx, node);
	while (child != XENT_NODE_INVALID && *count < max_count) {
		collect_focusable(ctx, child, out, count, max_count);
		child = xent_node_next(ctx, child);
	}
}

static XentCtx *s_sort_ctx = NULL;

static int      compare_tab_index(void const *a, void const *b) {
	XentNodeId na = *( XentNodeId const * ) a;
	XentNodeId nb = *( XentNodeId const * ) b;
	int32_t    ta = xent_get_tabindex(s_sort_ctx, na);
	int32_t    tb = xent_get_tabindex(s_sort_ctx, nb);

	int32_t    ea = (ta == 0) ? 0x7fffffff : ta;
	int32_t    eb = (tb == 0) ? 0x7fffffff : tb;

	if (ea != eb) return (ea < eb) ? -1 : 1;
	return (na < nb) ? -1 : (na > nb) ? 1 : 0;
}

static uint32_t
input_collect_tab_order(XentCtx *ctx, XentNodeId const *focusable, uint32_t count, XentNodeId *tab_order) {
	uint32_t tab_count = 0;
	for (uint32_t i = 0; i < count; i++) {
		int32_t ti = xent_get_tabindex(ctx, focusable [i]);
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

static void input_focus_node(FluxInput *input, XentNodeId node, bool suppress_visual) {
	if (node == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_store_find(input->store, node);
	if (!nd) return;

	input->focused           = node;
	nd->state.focused        = 1;
	nd->state.suppress_focus = suppress_visual;
	if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
}

static void input_move_focus(FluxInput *input, XentNodeId node, bool suppress_visual) {
	input_blur_focused(input);
	input_focus_node(input, node, suppress_visual);
}

static uint32_t
input_collect_focusable_siblings(XentCtx *ctx, XentNodeId parent, XentNodeId *siblings, uint32_t max_count) {
	uint32_t   count = 0;
	XentNodeId child = xent_node_first(ctx, parent);
	while (child != XENT_NODE_INVALID && count < max_count) {
		if (xent_get_focusable(ctx, child) && xent_sem_enabled(ctx, child)) siblings [count++] = child;
		child = xent_node_next(ctx, child);
	}
	return count;
}

static int input_arrow_delta(FluxKey direction) {
	if (direction == FLUX_KEY_LEFT || direction == FLUX_KEY_UP) return -1;
	if (direction == FLUX_KEY_RIGHT || direction == FLUX_KEY_DOWN) return 1;
	return 0;
}

static bool input_tab_hook(FluxInput *input, XentNodeId node, bool shift, bool entering) {
	FluxNodeData *nd = node == XENT_NODE_INVALID ? NULL : flux_store_find(input->store, node);
	return nd && nd->behavior.on_tab && nd->behavior.on_tab(nd->behavior.on_tab_ctx, shift, entering);
}

void flux_input_tab(FluxInput *input, XentNodeId root, bool shift) {
	if (!input) return;
	if (input_tab_hook(input, input->focused, shift, false)) return;
	if (input->modal_root != XENT_NODE_INVALID) root = input->modal_root;
	if (root == XENT_NODE_INVALID) return;

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
	input_move_focus(input, next_node, false);
	input_tab_hook(input, next_node, shift, true);
}

void flux_input_arrow(FluxInput *input, XentNodeId root, FluxKey direction) {
	if (!input || root == XENT_NODE_INVALID) return;
	if (input->focused == XENT_NODE_INVALID) return;

	XentNodeId parent = xent_node_parent(input->ctx, input->focused);
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
	input_move_focus(input, next_node, false);
}

void flux_input_activate(FluxInput *input) {
	if (!input || input->focused == XENT_NODE_INVALID) return;

	XtkHost *host = flux_store_host(input->store);
	if (host) {
		flux_input_route_activation(host, input->store, input->focused, XTK_EVENT_CAUSE_USER);
		return;
	}

	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (nd && nd->behavior.on_click) nd->behavior.on_click(nd->behavior.on_click_ctx);
}

static bool input_buttonbase(FluxControlType ct, bool *accepts_return) {
	*accepts_return = ct != FLUX_CONTROL_CHECKBOX && ct != FLUX_CONTROL_RADIO;
	return ct == FLUX_CONTROL_BUTTON
	    || ct == FLUX_CONTROL_TOGGLE_BUTTON
	    || ct == FLUX_CONTROL_CHECKBOX
	    || ct == FLUX_CONTROL_RADIO
	    || ct == FLUX_CONTROL_HYPERLINK
	    || ct == FLUX_CONTROL_DROPDOWN_BUTTON;
}

static void input_keypress_end(FluxInput *input) {
	FluxNodeData *nd = flux_store_find(input->store, input->key_pressed);
	if (nd) nd->state.pressed = 0;
	input->key_pressed = XENT_NODE_INVALID;
}

static bool input_checkbox_numpad(FluxInput *input, FluxNodeData *nd, FluxKey key) {
	FluxCheckboxData *cd = ( FluxCheckboxData * ) nd->component_data;
	if (!cd || cd->is_three_state) return false;
	bool want = key == FLUX_KEY_ADD;
	if ((cd->state == FLUX_CHECK_CHECKED) != want) flux_input_activate(input);
	return true;
}

static bool input_buttonkey_down(FluxInput *input, FluxNodeData *nd, FluxKey key, bool accepts_return) {
	bool press = key == FLUX_KEY_SPACE || (key == FLUX_KEY_ENTER && accepts_return);
	if (!press) {
		if (input->key_pressed != XENT_NODE_INVALID) input_keypress_end(input);
		return false;
	}
	if (input->key_pressed == XENT_NODE_INVALID && input->pressed == XENT_NODE_INVALID) {
		input->key_pressed   = input->focused;
		input->key_press_key = key;
		nd->state.pressed    = 1;
	}
	return true;
}

bool flux_input_buttonkey(FluxInput *input, FluxKey key, bool down) {
	if (!input || input->focused == XENT_NODE_INVALID) return false;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	bool          accepts_return;
	if (!nd || !input_buttonbase(nd->component_type, &accepts_return)) return false;
	if (!xent_sem_enabled(input->ctx, input->focused)) return key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER;
	if (down && nd->component_type == FLUX_CONTROL_CHECKBOX && (key == FLUX_KEY_ADD || key == FLUX_KEY_SUBTRACT))
		return input_checkbox_numpad(input, nd, key);
	if (down) return input_buttonkey_down(input, nd, key, accepts_return);
	if (input->key_pressed == XENT_NODE_INVALID || key != input->key_press_key) return false;
	bool click = input->key_pressed == input->focused && nd->state.pressed;
	input_keypress_end(input);
	if (click) flux_input_activate(input);
	return true;
}

void flux_input_escape(FluxInput *input) {
	if (!input) return;

	if (input->modal_root != XENT_NODE_INVALID && input->modal_escape) {
		input->modal_escape(input->modal_escape_ctx);
		return;
	}

	input_blur_focused(input);
	input->focused = XENT_NODE_INVALID;
}

void flux_input_setmodal(FluxInput *input, XentNodeId root, void (*on_escape)(void *ctx), void *ctx) {
	if (!input) return;
	input->modal_root       = root;
	input->modal_escape     = on_escape;
	input->modal_escape_ctx = ctx;
}

void flux_input_setfocus(FluxInput *input, XentNodeId node, bool programmatic) {
	if (!input) return;
	if (input->focused != node) {
		input_move_focus(input, node, programmatic);
		return;
	}
	FluxNodeData *nd = programmatic ? flux_store_find(input->store, node) : NULL;
	if (nd) nd->state.suppress_focus = 1;
}
