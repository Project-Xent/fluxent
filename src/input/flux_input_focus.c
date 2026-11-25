#include "flux_input_internal.h"
#include "bridge/flux_internal.h"
#include "window/flux_window_internal.h"

#include <fluxent/flux_node_store.h>
#include "store/flux_node_data.h"
#include <stdlib.h>

static bool input_node_is_visible_focusable(XentCtx *ctx, XentNodeId node) {
	if (!xent_get_focusable(ctx, node) || !xent_sem_enabled(ctx, node)) return false;

	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);
	return rect.w > 0.0f && rect.h > 0.0f;
}

typedef struct InputTabStop {
	XentNodeId node;
	int32_t    index;
	uint32_t   order;
} InputTabStop;

typedef struct InputTabOrder {
	InputTabStop *stops;
	uint32_t      count;
	uint32_t      capacity;
} InputTabOrder;

static bool input_append_tab(InputTabOrder *tabs, XentNodeId node, int32_t index) {
	if (tabs->count == tabs->capacity) {
		uint32_t cap = tabs->capacity ? tabs->capacity * 2u : 32u;
		if (cap <= tabs->capacity) return false;
		InputTabStop *stops = realloc(tabs->stops, cap * sizeof(*stops));
		if (!stops) return false;
		tabs->stops    = stops;
		tabs->capacity = cap;
	}
	tabs->stops [tabs->count] = (InputTabStop) {node, index == 0 ? INT32_MAX : index, tabs->count};
	tabs->count++;
	return true;
}

static bool input_available(XentCtx const *ctx, XentNodeId node) {
	return !xent_collapsed(ctx, node) && xent_sem_enabled(ctx, node);
}

bool input_focus_available(XentCtx const *ctx, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return false;
	for (; node != XENT_NODE_INVALID; node = xent_node_parent(ctx, node))
		if (!input_available(ctx, node)) return false;
	return true;
}

static bool input_collect_tabstop(XentCtx *ctx, XentNodeId node, InputTabOrder *tabs) {
	int32_t index = xent_get_tabindex(ctx, node);
	return index < 0 || !input_node_is_visible_focusable(ctx, node) || input_append_tab(tabs, node, index);
}

static XentNodeId input_tab_child(XentCtx const *ctx, XentNodeId node, XentNodeId child, bool reverse) {
	if (child == XENT_NODE_INVALID) return reverse ? xent_node_last(ctx, node) : xent_node_first(ctx, node);
	return reverse ? xent_node_prev(ctx, child) : xent_node_next(ctx, child);
}

static bool input_collect_tabs(FluxInput *input, XentNodeId node, InputTabOrder *tabs) {
	if (!input_available(input->ctx, node)) return true;
	FluxNodeData const *nd = flux_store_find(input->store, node);
	if (nd && nd->tab_inert) return true;
	if (!input_collect_tabstop(input->ctx, node, tabs)) return false;
	bool reverse = nd && nd->tab_reverse_children;
	for (XentNodeId child = input_tab_child(input->ctx, node, XENT_NODE_INVALID, reverse); child != XENT_NODE_INVALID;
	  child               = input_tab_child(input->ctx, node, child, reverse))
	{
		if (!input_collect_tabs(input, child, tabs)) return false;
	}
	return true;
}

static int compare_tab_index(void const *a, void const *b) {
	InputTabStop const *left = a, *right = b;
	if (left->index != right->index) return left->index < right->index ? -1 : 1;
	return (left->order > right->order) - (left->order < right->order);
}

static int input_find_tab_index(InputTabStop const *stops, uint32_t count, XentNodeId target) {
	for (uint32_t i = 0; i < count; i++)
		if (stops [i].node == target) return ( int ) i;
	return -1;
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
		if (xent_get_focusable(ctx, child) && xent_sem_enabled(ctx, child) && !xent_collapsed(ctx, child))
			siblings [count++] = child;
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

static InputTabOrder input_sorted_tab_order(FluxInput *input, XentNodeId root) {
	InputTabOrder tabs = {0};
	if (!input_collect_tabs(input, root, &tabs)) {
		free(tabs.stops);
		return (InputTabOrder) {0};
	}
	if (tabs.count > 1) qsort(tabs.stops, tabs.count, sizeof(*tabs.stops), compare_tab_index);
	return tabs;
}

bool input_within(XentCtx const *ctx, XentNodeId node, XentNodeId ancestor) {
	for (; node != XENT_NODE_INVALID; node = xent_node_parent(ctx, node))
		if (node == ancestor) return true;
	return false;
}

static XentNodeId input_tab_group(FluxInput *input, XentNodeId node) {
	for (; node != XENT_NODE_INVALID; node = xent_node_parent(input->ctx, node)) {
		FluxNodeData const *nd = flux_store_find(input->store, node);
		if (nd && nd->behavior.tab_entry) return node;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId input_tab_scope(FluxInput *input, XentNodeId root) {
	if (input->modal_root != XENT_NODE_INVALID) root = input->modal_root;
	if (!input_within(input->ctx, input->focused, root)) return root;
	for (XentNodeId node = input->focused; node != root; node = xent_node_parent(input->ctx, node)) {
		FluxNodeData const *nd = flux_store_find(input->store, node);
		if (nd && nd->tab_cycle) return node;
	}
	return root;
}

static XentNodeId
input_tab_entry(FluxInput *input, XentNodeId group, XentNodeId candidate, InputTabStop const *stops, uint32_t count) {
	if (group == XENT_NODE_INVALID) return candidate;
	FluxNodeData const *nd        = flux_store_find(input->store, group);
	XentNodeId          preferred = nd->behavior.tab_entry(nd->behavior.tab_entry_ctx, group);
	return input_find_tab_index(stops, count, preferred) >= 0 ? preferred : candidate;
}

static XentNodeId input_next_tab(FluxInput *input, InputTabStop const *stops, uint32_t count, bool shift) {
	XentNodeId origin = input_tab_group(input, input->focused);
	int        index  = input_find_tab_index(stops, count, input->focused);
	for (uint32_t i = 0; i < count; i++) {
		index                = input_next_tab_index(index, count, shift);
		XentNodeId candidate = stops [index].node;
		XentNodeId group     = input_tab_group(input, candidate);
		if (origin == XENT_NODE_INVALID || group != origin)
			return input_tab_entry(input, group, candidate, stops, count);
	}
	return XENT_NODE_INVALID;
}

void flux_input_tab(FluxInput *input, XentNodeId root, bool shift) {
	if (!input) return;
	if (input_tab_hook(input, input->focused, shift, false)) return;
	root = input_tab_scope(input, root);
	if (root == XENT_NODE_INVALID) return;
	InputTabOrder tabs = input_sorted_tab_order(input, root);
	XentNodeId    next = input_next_tab(input, tabs.stops, tabs.count, shift);
	free(tabs.stops);
	if (next == XENT_NODE_INVALID) return;
	input_move_focus(input, next, false);
	input_tab_hook(input, next, shift, true);
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

static bool input_buttonkey_up(FluxInput *input, FluxNodeData *nd, FluxKey key) {
	if (input->key_pressed == XENT_NODE_INVALID || key != input->key_press_key) return false;
	bool click = input->key_pressed == input->focused && nd->state.pressed;
	input_keypress_end(input);
	if (click) flux_input_activate(input);
	return true;
}

bool flux_input_keyactivate(FluxInput *input, FluxKey key, bool down) {
	if (!input || (key != FLUX_KEY_SPACE && key != FLUX_KEY_ENTER)) return false;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (!nd || !xent_sem_enabled(input->ctx, input->focused)) return true;
	return down ? input_buttonkey_down(input, nd, key, true) : input_buttonkey_up(input, nd, key);
}

bool flux_input_keyrepeated(FluxInput const *input) { return input && input->window && input->window->key_repeated; }

bool flux_input_buttonkey(FluxInput *input, FluxKey key, bool down) {
	if (!input || input->focused == XENT_NODE_INVALID) return false;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	bool          accepts_return;
	if (!nd || !input_buttonbase(nd->component_type, &accepts_return)) return false;
	if (!xent_sem_enabled(input->ctx, input->focused)) return key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER;
	if (down && nd->component_type == FLUX_CONTROL_CHECKBOX && (key == FLUX_KEY_ADD || key == FLUX_KEY_SUBTRACT))
		return input_checkbox_numpad(input, nd, key);
	if (down) return input_buttonkey_down(input, nd, key, accepts_return);
	return input_buttonkey_up(input, nd, key);
}

void flux_input_escape(FluxInput *input) {
	if (!input) return;

	if (input->modal_root != XENT_NODE_INVALID && input->modal_escape) {
		input->modal_escape(input->modal_escape_ctx);
		return;
	}

	input_blur_focused(input);
	input->focused = XENT_NODE_INVALID;
	input_xy_reset(input);
}

void flux_input_setmodal(FluxInput *input, XentNodeId root, void (*on_escape)(void *ctx), void *ctx) {
	if (!input) return;
	if (root != XENT_NODE_INVALID && input->modal_root == XENT_NODE_INVALID) {
		input->xy_saved = input->xy;
		input_xy_reset(input);
	}
	if (root == XENT_NODE_INVALID && input->modal_root != XENT_NODE_INVALID) input->xy = input->xy_saved;
	input->modal_root       = root;
	input->modal_escape     = on_escape;
	input->modal_escape_ctx = ctx;
	input->modal_enter      = NULL;
}

void flux_input_setmodal_enter(FluxInput *input, bool (*on_enter)(void *ctx)) {
	if (input) input->modal_enter = on_enter;
}

bool flux_input_modal_enter(FluxInput *input) {
	return input->modal_enter && input->modal_enter(input->modal_escape_ctx);
}

void flux_input_setfocus(FluxInput *input, XentNodeId node, bool programmatic) {
	if (!input) return;
	if (input->focused != node) {
		input_move_focus(input, node, programmatic);
		return;
	}
	FluxNodeData *nd = flux_store_find(input->store, node);
	if (nd) nd->state.suppress_focus = programmatic;
}

bool flux_input_focuswithin(FluxInput *input, XentNodeId node, bool programmatic) {
	if (!input || node == XENT_NODE_INVALID || !xent_node_valid(input->ctx, node)) return false;
	if (input->modal_root != XENT_NODE_INVALID && !input_within(input->ctx, node, input->modal_root)) return false;
	if (!input_focus_available(input->ctx, node)) return false;
	if (input_within(input->ctx, input->focused, node) && input_focus_available(input->ctx, input->focused))
		return true;

	XentNodeId target = node;
	if (!input_node_is_visible_focusable(input->ctx, node)) {
		InputTabOrder tabs = input_sorted_tab_order(input, node);
		target             = tabs.count ? tabs.stops [0].node : XENT_NODE_INVALID;
		free(tabs.stops);
	}
	if (target == XENT_NODE_INVALID) return false;
	flux_input_setfocus(input, target, programmatic);
	return true;
}
