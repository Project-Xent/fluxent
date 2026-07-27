/**
 * @file test_fx_ime.c
 * @brief Backend/input IME routing: composition reaches the focused node.
 * IME is not a Core concern; this locks the Fluxent input contract.
 */
#include <fluxent/flux_input.h>
#include <fluxent/flux_node_store.h>
#include "controls/factory/flux_init_textbox.h"
#include "render/flux_fluent.h"
#include "store/flux_node_data.h"
#include "window/flux_window_internal.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

typedef struct {
	uint32_t calls;
	uint32_t last_len;
	uint32_t last_cursor;
	wchar_t  last [32];
} ImeProbe;

typedef struct EditProbe {
	uint32_t calls;
	char     last [64];
} EditProbe;

static void on_ime(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor) {
	ImeProbe *p = ( ImeProbe * ) ctx;
	p->calls++;
	p->last_len    = len;
	p->last_cursor = cursor;
	if (!text || len == 0u) {
		p->last [0] = 0;
		return;
	}
	uint32_t n = len < 31u ? len : 31u;
	memcpy(p->last, text, sizeof(wchar_t) * n);
	p->last [n] = 0;
}

static void edit_changed(void *ctx, char const *text) {
	EditProbe *probe = ctx;
	probe->calls++;
	snprintf(probe->last, sizeof(probe->last), "%s", text ? text : "");
}

typedef struct DropTextBoxProbe {
	FluxStore *store;
	XentNodeId node;
	uint32_t   calls;
	uint32_t   after_drop_calls;
} DropTextBoxProbe;

static void drop_input_node(void *ctx, XentNodeId node) { flux_input_dropnode(( FluxInput * ) ctx, node); }

static void drop_textbox_on_change(void *ctx, char const *text) {
	DropTextBoxProbe *probe = ( DropTextBoxProbe * ) ctx;
	if (text && strcmp(text, "x") == 0) probe->calls++;
	flux_tree_drop(probe->store, probe->node);
}

static void drop_node_on_pointer_move(void *ctx, float x, float y) {
	( void ) x;
	( void ) y;
	DropTextBoxProbe *probe = ( DropTextBoxProbe * ) ctx;
	probe->calls++;
	flux_tree_drop(probe->store, probe->node);
}

static void count_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) x;
	( void ) y;
	( void ) clicks;
	(( DropTextBoxProbe * ) ctx)->after_drop_calls++;
}

static int test_pointer_callback_can_drop_node(XentCtx *ctx, FluxStore *store, FluxInput *input) {
	XentNodeId root = xent_node_create(ctx);
	XentNodeId node = xent_node_create(ctx);
	EXPECT(root != XENT_NODE_INVALID && node != XENT_NODE_INVALID, "pointer nodes");
	xent_node_append(ctx, root, node);
	xent_setsize(ctx, root, (XentSize) {100.0f, 100.0f});
	xent_setsize(ctx, node, (XentSize) {100.0f, 100.0f});
	xent_set_focusable(ctx, node, true);

	DropTextBoxProbe probe = {.store = store, .node = node};
	FluxNodeData    *data  = flux_store_ensure(store, node);
	EXPECT(data, "pointer sidecar");
	data->behavior.on_pointer_move     = drop_node_on_pointer_move;
	data->behavior.on_pointer_move_ctx = &probe;
	data->behavior.on_pointer_down     = count_pointer_down;
	data->behavior.on_pointer_down_ctx = &probe;
	EXPECT(xent_layout(ctx, root, 100.0f, 100.0f), "pointer layout");

	flux_store_ondel(store, drop_input_node, input);
	FluxPointerEvent event = {
	  .kind           = FLUX_POINTER_DOWN,
	  .device         = FLUX_POINTER_MOUSE,
	  .x              = 10.0f,
	  .y              = 10.0f,
	  .changed_button = FLUX_POINTER_BUTTON_LEFT,
	};
	flux_input_dispatch(input, root, &event);
	EXPECT(probe.calls == 1u, "pointer move callback ran");
	EXPECT(probe.after_drop_calls == 0u, "pointer dispatch stops after node destruction");
	EXPECT(flux_input_pressed(input) == XENT_NODE_INVALID, "destroyed pointer target leaves no press");
	EXPECT(flux_input_focus(input) == XENT_NODE_INVALID, "destroyed pointer target leaves no focus");
	flux_store_ondel(store, NULL, NULL);
	flux_tree_drop(store, root);
	return 0;
}

static int test_textbox_callback_can_drop_node(XentCtx *ctx, FluxStore *store, FluxInput *input) {
	XentNodeId root = xent_node_create(ctx);
	EXPECT(root != XENT_NODE_INVALID, "textbox root");

	DropTextBoxProbe probe = {.store = store};
	probe.node             = flux_textbox_init(&(FluxTextBoxDesc) {
	  .ctx = ctx, .store = store, .parent = root, .on_change = drop_textbox_on_change, .userdata = &probe});
	EXPECT(probe.node != XENT_NODE_INVALID, "textbox");

	flux_store_ondel(store, drop_input_node, input);
	flux_input_setfocus(input, probe.node);
	flux_input_char(input, L'x');
	EXPECT(probe.calls == 1u, "textbox reports edit before teardown");
	EXPECT(!xent_node_valid(ctx, probe.node), "textbox callback may drop itself");
	EXPECT(flux_input_focus(input) == XENT_NODE_INVALID, "destroyed textbox leaves no focus");
	flux_store_ondel(store, NULL, NULL);
	flux_tree_drop(store, root);
	return 0;
}

static int test_textbox_editing(XentCtx *ctx, FluxStore *store, FluxInput *input) {
	XentNodeId root = xent_node_create(ctx);
	EXPECT(root != XENT_NODE_INVALID, "edit root");

	EditProbe  probe = {0};
	XentNodeId box   = flux_textbox_init(&(FluxTextBoxDesc) {
	  .ctx = ctx, .store = store, .parent = root, .on_change = edit_changed, .userdata = &probe});
	EXPECT(box != XENT_NODE_INVALID, "edit textbox");
	flux_input_setfocus(input, box);

	flux_input_char(input, L'a');
	flux_input_char(input, L'b');
	EXPECT(probe.calls == 2u && strcmp(probe.last, "ab") == 0, "characters append");

	EXPECT(flux_input_keydown(input, FLUX_KEY_LEFT), "left key handled");
	flux_input_char(input, L'x');
	EXPECT(strcmp(probe.last, "axb") == 0, "caret insertion");
	EXPECT(flux_input_keydown(input, FLUX_KEY_BACKSPACE), "backspace handled");
	EXPECT(strcmp(probe.last, "ab") == 0, "backspace removes prior grapheme");

	FluxNodeData    *node = flux_store_find(store, box);
	FluxTextBoxData *data = node ? node->component_data : NULL;
	EXPECT(data && data->cursor_position == 1u, "caret survives edits");
	flux_input_ime(input, L"ni", 2u, 1u);
	EXPECT(data->composition_length == 2u && data->composition_cursor == 1u, "textbox composition retained");
	flux_input_imeend(input);
	EXPECT(data->composition_length == 0u, "textbox composition clears");

	flux_input_setfocus(input, XENT_NODE_INVALID);
	EXPECT(flux_input_focus(input) == XENT_NODE_INVALID, "programmatic focus clear");
	flux_tree_drop(store, root);
	return 0;
}

static int test_ime_anchor_refreshes_after_layout(void) {
	FluxAppCfg cfg = {.width = 320, .height = 200};
	FluxApp   *app = flux_app_create(&cfg);
	EXPECT(app, "IME layout app");

	XentCtx   *ctx   = flux_app_ctx(app);
	FluxStore *store = flux_app_store(app);
	XentNodeId root  = flux_app_root(app);
	xent_setp(ctx, root, (XentInsets) {37.0f, 41.0f, 0.0f, 0.0f});
	XentNodeId box = flux_app_create_textbox(&(FluxAppTextBoxDesc) {.app = app, .parent = root});
	EXPECT(box != XENT_NODE_INVALID, "IME layout textbox");

	FluxWindow *window = flux_app_window(app);
	flux_input_setfocus(flux_app_input(app), box);
	EXPECT(!window->ime_position_valid, "pre-layout IME anchor remains unset");
	window->on_render(window->render_ctx);
	EXPECT(window->ime_position_valid, "layout refreshes focused IME anchor");

	XentRect rect = {0};
	EXPECT(xent_layout_rect(ctx, box, &rect), "IME layout rect");
	FluxDpi dpi = flux_window_dpi(window);
	EXPECT(
	  window->ime_x == ( int ) ((rect.x + FLUX_TEXTBOX_PAD_L) * dpi.dpi_x / FLUX_DPI_BASE), "IME x follows layout"
	);
	EXPECT(
	  window->ime_y == ( int ) ((rect.y + FLUX_TEXTBOX_PAD_T) * dpi.dpi_y / FLUX_DPI_BASE), "IME y follows layout"
	);

	int old_y = window->ime_y;
	xent_setp(ctx, root, (XentInsets) {37.0f, 73.0f, 0.0f, 0.0f});
	window->on_render(window->render_ctx);
	EXPECT(window->ime_y > old_y, "IME anchor follows later layout movement");

	flux_app_destroy(app);
	return 0;
}

int main(void) {
	XentCtx   *ctx   = xent_ctx_create(NULL);
	FluxStore *store = flux_store_create(16);
	EXPECT(ctx && store, "context/store");
	flux_store_bind(store, ctx);

	FluxInput *input = flux_input_create(ctx, store);
	EXPECT(input, "input");

	XentNodeId node = xent_node_create(ctx);
	EXPECT(node != XENT_NODE_INVALID, "node");
	FluxNodeData *nd = flux_store_ensure(store, node);
	EXPECT(nd, "sidecar");
	flux_node_settype(store, node, FLUX_CONTROL_TEXT_INPUT);

	ImeProbe probe                      = {0};
	nd->behavior.on_ime_composition     = on_ime;
	nd->behavior.on_ime_composition_ctx = &probe;

	flux_input_ime(input, L"ni", 2u, 2u);
	EXPECT(probe.calls == 0u, "no focus => no IME delivery");

	flux_input_setfocus(input, node);
	EXPECT(flux_input_focus(input) == node, "focused");

	flux_input_ime(input, L"ni", 2u, 2u);
	EXPECT(probe.calls == 1u, "composition delivered");
	EXPECT(probe.last_len == 2u && probe.last_cursor == 2u, "payload");
	EXPECT(wcscmp(probe.last, L"ni") == 0, "text");

	flux_input_imeend(input);
	EXPECT(probe.calls == 2u && probe.last_len == 0u, "ime end clears composition");

	if (test_textbox_callback_can_drop_node(ctx, store, input)) return 1;
	if (test_pointer_callback_can_drop_node(ctx, store, input)) return 1;
	if (test_textbox_editing(ctx, store, input)) return 1;

	flux_input_destroy(input);
	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	if (test_ime_anchor_refreshes_after_layout()) return 1;
	printf("PASS: fluxent ime input routing\n");
	return 0;
}
