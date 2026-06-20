/**
 * @file test_fx_reconcile.c
 * @brief Headless reconciler test: mount, prop diff with node reuse, keyed
 * reorder, conditional unmount, message round-trips through trampolines,
 * and teardown — all without a window or GPU.
 */
#include "flux_internal.h"

#include <stdio.h>
#include <string.h>

enum
{
	MSG_NONE = 0,
	MSG_INCREMENT,
	MSG_TOGGLE,
	MSG_SLIDE,
	MSG_REVERSE,
};

typedef struct Model {
	int   count;
	bool  show_extra;
	bool  reversed;
	float value;
} Model;

static void update(void *model, XtkMsg msg) {
	Model *m = ( Model * ) model;
	switch (msg.tag) {
	case MSG_INCREMENT : m->count++; break;
	case MSG_TOGGLE    : m->show_extra = msg.b; break;
	case MSG_SLIDE     : m->value = msg.f; break;
	case MSG_REVERSE   : m->reversed = !m->reversed; break;
	default            : break;
	}
}

static XtkEl *view(XtkUi *ui, void *model) {
	Model      *m      = ( Model * ) model;
	char const *first  = m->reversed ? "beta" : "alpha";
	char const *second = m->reversed ? "alpha" : "beta";

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8},
	  (XtkEl *[]) {
		xtk_text(ui, xtk_fmt(ui, "Count: %d", m->count), (XtkTextDesc) {.size = 20}),
		xtk_button(ui, m->count > 2 ? "Lots!" : "Increment", (XtkButtonDesc) {.on_click = xtk_msg(MSG_INCREMENT)}),
		xtk_checkbox(ui, "Extra", (XtkToggleDesc) {.checked = m->show_extra, .on_change = xtk_msg(MSG_TOGGLE)}),
		m->show_extra ? xtk_slider(ui, (XtkSliderDesc) {.value = m->value, .on_change = xtk_msg(MSG_SLIDE)}) : NULL,
		xtk_keyed(ui, first, xtk_text(ui, first, (XtkTextDesc) {0})),
		xtk_keyed(ui, second, xtk_text(ui, second, (XtkTextDesc) {0})), XTK_END}
	);
}

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static FluxNodeData *node_data(FluxNodeStore *store, XtkNode *n) { return flux_node_store_get(store, n->node); }

int                  main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	EXPECT(ctx && store, "context/store creation");
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	Model      m    = {0};

	FluxBackendCtx bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend     be   = flux_xtk_backend(&bctx);
	XtkRuntime    *rt   = xtk_runtime_create(ctx, &be, host, &m, update, view);
	EXPECT(rt, "runtime creation");
	bctx.runtime = rt;

	xtk_runtime_frame(rt);
	EXPECT(rt->root, "initial mount");
	EXPECT(rt->root->child_count == 5, "mounted children (slider hidden)");
	EXPECT(xent_get_child_count(ctx, host) == 1, "root attached to host");

	XtkNode        *btn        = rt->root->children [1];
	XentNodeId      btn_id     = btn->node;
	XentNodeId      alpha_id   = rt->root->children [3]->node;
	XentNodeId      beta_id    = rt->root->children [4]->node;
	uint32_t        base_count = flux_node_store_count(store);

	FluxButtonData *bd         = ( FluxButtonData * ) node_data(store, btn)->component_data;
	EXPECT(strcmp(bd->label, "Increment") == 0, "button label mounted");

	/* Simulate clicks through the retained behavior (the real input path). */
	FluxNodeData *bnd = node_data(store, btn);
	bnd->behavior.on_click(bnd->behavior.on_click_ctx);
	bnd->behavior.on_click(bnd->behavior.on_click_ctx);
	bnd->behavior.on_click(bnd->behavior.on_click_ctx);
	xtk_runtime_frame(rt);

	EXPECT(m.count == 3, "messages drained through update");
	EXPECT(rt->root->children [1]->node == btn_id, "button node reused across diffs");
	bd = ( FluxButtonData * ) node_data(store, rt->root->children [1])->component_data;
	EXPECT(strcmp(bd->label, "Lots!") == 0, "label updated in place");
	EXPECT(flux_node_store_count(store) == base_count, "no node churn on prop diff");

	/* Conditional mount: checkbox toggle reveals the slider. */
	FluxNodeData *cnd = node_data(store, rt->root->children [2]);
	cnd->behavior.on_click(cnd->behavior.on_click_ctx);
	xtk_runtime_frame(rt);
	EXPECT(m.show_extra, "toggle message carried new state");
	EXPECT(rt->root->child_count == 6, "slider mounted");
	EXPECT(flux_node_store_count(store) == base_count + 1, "exactly one node added");

	XentNodeId slider_id = rt->root->children [3]->node;
	EXPECT(flux_get_control_type(ctx, slider_id) == XTK_CONTROL_SLIDER, "slider in conditional slot");

	/* Keyed reorder: alpha/beta swap must move nodes, not recreate them. */
	xtk_runtime_post(rt, xtk_msg(MSG_REVERSE));
	xtk_runtime_frame(rt);
	EXPECT(rt->root->children [4]->node == beta_id, "keyed beta moved, same node");
	EXPECT(rt->root->children [5]->node == alpha_id, "keyed alpha moved, same node");
	EXPECT(flux_node_store_count(store) == base_count + 1, "reorder created nothing");

	/* xent sibling order must match the new child order. */
	XentNodeId expect_order [6];
	for (int i = 0; i < 6; i++) expect_order [i] = rt->root->children [i]->node;
	XentNodeId walk = xent_get_first_child(ctx, rt->root->node);
	for (int i = 0; i < 6; i++) {
		EXPECT(walk == expect_order [i], "xent order matches element order");
		walk = xent_get_next_sibling(ctx, walk);
	}

	/* Conditional unmount: hide the slider again. */
	cnd = node_data(store, rt->root->children [2]);
	cnd->behavior.on_click(cnd->behavior.on_click_ctx);
	xtk_runtime_frame(rt);
	EXPECT(rt->root->child_count == 5, "slider unmounted");
	EXPECT(flux_node_store_count(store) == base_count, "slider node fully released");
	EXPECT(!xent_is_valid_node(ctx, slider_id), "slider dead in xent");

	for (int i = 0; i < 2000; i++) {
		xtk_runtime_post(rt, xtk_msg(MSG_INCREMENT));
		xtk_runtime_frame(rt);
	}
	EXPECT(m.count == 2003, "2000 message frames");
	EXPECT(flux_node_store_count(store) == base_count, "long run is churn-free");

	xtk_runtime_destroy(rt);
	EXPECT(flux_node_store_count(store) <= 1, "teardown drained the store");

	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: xtk reconciler\n");
	return 0;
}
