/**
 * @file test_subtree.c
 * @brief Headless test for flux_subtree_destroy and the node-removed listener.
 *
 * Builds a controls subtree (no window, no GPU), destroys it, and asserts the
 * store drained, the layout nodes died, and the listener saw every node.
 * Repeats the cycle many times as a crash/leak smoke test.
 */
#include <fluxent/fluxent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_removed_count;

static void count_removed(void *userdata, XentNodeId id) {
	( void ) id;
	FluxInput *input = ( FluxInput * ) userdata;
	flux_input_node_destroyed(input, id);
	g_removed_count++;
}

static XentNodeId build_page(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	XentNodeId page = flux_create_scroll(&(FluxContainerCreateInfo) {ctx, store, root});

	flux_create_text(&(FluxTextCreateInfo) {ctx, store, page, "Title", 24.0f});
	flux_create_button(&(FluxButtonCreateInfo) {ctx, store, page, "Button", NULL, NULL});
	flux_create_checkbox(&(FluxToggleCreateInfo) {ctx, store, page, "Check", false, NULL, NULL});
	flux_create_slider(&(FluxSliderCreateInfo) {ctx, store, page, 0.0f, 1.0f, 0.5f, NULL, NULL});
	flux_create_textbox(&(FluxTextBoxCreateInfo) {ctx, store, page, "...", NULL, NULL});
	flux_create_progress(&(FluxProgressCreateInfo) {ctx, store, page, 1.0f, 2.0f});

	XentNodeId card = flux_create_card(&(FluxContainerCreateInfo) {ctx, store, page});
	flux_create_text(&(FluxTextCreateInfo) {ctx, store, card, "Nested", 13.0f});
	flux_create_button(&(FluxButtonCreateInfo) {ctx, store, card, "Nested button", NULL, NULL});

	return page;
}

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

int main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	EXPECT(ctx && store, "context/store creation");

	flux_node_store_bind_context(store, ctx);

	FluxInput *input = flux_input_create(ctx, store);
	EXPECT(input, "input creation");
	flux_node_store_set_remove_listener(store, count_removed, input);

	XentNodeId root     = xent_create_node(ctx);

	uint32_t   baseline = flux_node_store_count(store);
	XentNodeId page     = build_page(ctx, store, root);
	uint32_t   built    = flux_node_store_count(store);
	EXPECT(built > baseline, "store grew while building");

	XentNodeId inner_button = xent_get_first_child(ctx, page);
	flux_input_set_focus(input, inner_button);

	g_removed_count = 0;
	flux_subtree_destroy(store, page);

	EXPECT(flux_node_store_count(store) == baseline, "store drained back to baseline");
	EXPECT(!xent_is_valid_node(ctx, page), "page node freed in xent");
	EXPECT(( uint32_t ) g_removed_count == built - baseline, "listener saw every node");
	EXPECT(flux_input_get_focused(input) == XENT_NODE_INVALID, "focus reference dropped");
	EXPECT(xent_get_child_count(ctx, root) == 0, "root detached from destroyed child");

	for (int i = 0; i < 1000; i++) {
		XentNodeId p = build_page(ctx, store, root);
		flux_subtree_destroy(store, p);
	}
	EXPECT(flux_node_store_count(store) == baseline, "1000 cycles leave store clean");

	char volatile_label [32];
	snprintf(volatile_label, sizeof(volatile_label), "Transient %d", 42);
	XentNodeId btn = flux_create_button(&(FluxButtonCreateInfo) {ctx, store, root, volatile_label, NULL, NULL});
	memset(volatile_label, 'X', sizeof(volatile_label) - 1);

	FluxNodeData   *bnd = flux_node_store_get(store, btn);
	FluxButtonData *bd  = bnd ? ( FluxButtonData * ) bnd->component_data : NULL;
	EXPECT(bd && bd->label && strcmp(bd->label, "Transient 42") == 0, "control owns a copy of its label");
	flux_subtree_destroy(store, btn);

	flux_input_destroy(input);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: subtree destroy (%d total node removals across 1001 cycles)\n", g_removed_count);
	return 0;
}
