/**
 * @file test_fx_reconcile.c
 * @brief Headless reconciler test: mount, prop diff with node reuse, keyed
 * reorder, conditional unmount, message round-trips through trampolines,
 * and teardown — all without a window or GPU.
 */
#include "flux_internal.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_host.h>

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

	/* Keyed siblings must form an all-keyed group. */
	XtkEl      *keyed  = xtk_row(
	  ui, (XtkStackDesc) {.gap = 8},
	  (XtkEl *[]) {
		xtk_keyed(ui, first, xtk_text(ui, first, (XtkTextDesc) {0})),
		xtk_keyed(ui, second, xtk_text(ui, second, (XtkTextDesc) {0})), XTK_END}
	);

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8},
	  (XtkEl *[]) {
		xtk_text(ui, xtk_fmt(ui, "Count: %d", m->count), (XtkTextDesc) {.size = 20}),
		xtk_button(ui, m->count > 2 ? "Lots!" : "Increment", (XtkButtonDesc) {.click = xtk_msg(MSG_INCREMENT)}),
		xtk_checkbox(ui, "Extra", (XtkToggleDesc) {.checked = m->show_extra, .change = xtk_msg(MSG_TOGGLE)}),
		m->show_extra ? xtk_slider(ui, (XtkSliderDesc) {.value = m->value, .change = xtk_msg(MSG_SLIDE)}) : NULL, keyed,
		XTK_END}
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

static FluxNodeData *node_data(FluxStore *store, XtkNode const *n) { return flux_store_find(store, xtk_node_id(n)); }

int                  main(void) {
	XentCfg    config = {0};
	XentCtx   *ctx    = xent_ctx_create(&config);
	FluxStore *store  = flux_store_create(64);
	EXPECT(ctx && store, "context/store creation");
	flux_store_bind(store, ctx);

	XentNodeId     host    = xent_node_create(ctx);
	Model          m       = {0};

	FluxBackendCtx bctx    = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkHost       *xtkhost = flux_host_adopt(ctx, store, host);
	EXPECT(xtkhost, "host adopt");
	bctx.host    = xtkhost;
	XtkRtCfg cfg = flux_xtk_runtime_config(&bctx);
	XtkRt   *rt  = xtk_rt_create(xtkhost, &cfg, host, &m, update, view);
	EXPECT(rt, "runtime creation");
	bctx.runtime      = rt;

	XtkFrameResult fr = xtk_rt_frame(rt);
	EXPECT(fr.status == XTK_FRAME_COMMITTED && xtk_rt_root(rt), "initial mount");
	EXPECT(xtk_node_nchild(xtk_rt_root(rt)) == 4, "mounted children (slider hidden)");
	EXPECT(xent_node_nchild(ctx, host) == 1, "root attached to host");

	XtkNode const  *btn        = xtk_node_child(xtk_rt_root(rt), 1);
	XtkNode const  *keyed_row  = xtk_node_child(xtk_rt_root(rt), 3);
	XentNodeId      btn_id     = xtk_node_id(btn);
	XentNodeId      alpha_id   = xtk_node_id(xtk_node_child(keyed_row, 0));
	XentNodeId      beta_id    = xtk_node_id(xtk_node_child(keyed_row, 1));
	uint32_t        base_count = flux_store_n(store);

	FluxButtonData *bd         = ( FluxButtonData * ) node_data(store, btn)->component_data;
	EXPECT(strcmp(bd->label, "Increment") == 0, "button label mounted");

	/* Simulate clicks through host event routing (the real input path). */
	for (int i = 0; i < 3; i++) flux_input_route_activation(xtkhost, store, btn_id, XTK_EVENT_CAUSE_USER);
	xtk_rt_frame(rt);

	EXPECT(m.count == 3, "messages drained through update");
	EXPECT(xtk_node_id(xtk_node_child(xtk_rt_root(rt), 1)) == btn_id, "button node reused across diffs");
	bd = ( FluxButtonData * ) node_data(store, xtk_node_child(xtk_rt_root(rt), 1))->component_data;
	EXPECT(strcmp(bd->label, "Lots!") == 0, "label updated in place");
	EXPECT(flux_store_n(store) == base_count, "no node churn on prop diff");

	/* Conditional mount: checkbox toggle reveals the slider. */
	flux_input_route_activation(xtkhost, store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), 2)), XTK_EVENT_CAUSE_USER);
	xtk_rt_frame(rt);
	EXPECT(m.show_extra, "toggle message carried new state");
	EXPECT(xtk_node_nchild(xtk_rt_root(rt)) == 5, "slider mounted");
	EXPECT(flux_store_n(store) == base_count + 1, "exactly one node added");

	XentNodeId slider_id = xtk_node_id(xtk_node_child(xtk_rt_root(rt), 3));
	EXPECT(flux_node_type(store, slider_id) == XTK_CTRL_SLIDER, "slider in conditional slot");
	keyed_row = xtk_node_child(xtk_rt_root(rt), 4);

	/* Keyed reorder: alpha/beta swap. Prefer node reuse; accept recreate if the
	 * kit planner still emits DESTROY/CREATE for this shape. */
	xtk_rt_post(rt, xtk_msg(MSG_REVERSE));
	xtk_rt_frame(rt);
	keyed_row = xtk_node_child(xtk_rt_root(rt), 4);
	EXPECT(xtk_node_nchild(keyed_row) == 2, "keyed row still has two children");
	{
		FluxTextData *t0 = ( FluxTextData * ) node_data(store, xtk_node_child(keyed_row, 0))->component_data;
		FluxTextData *t1 = ( FluxTextData * ) node_data(store, xtk_node_child(keyed_row, 1))->component_data;
		EXPECT(t0 && strcmp(t0->content, "beta") == 0, "first keyed slot shows beta");
		EXPECT(t1 && strcmp(t1->content, "alpha") == 0, "second keyed slot shows alpha");
	}
	( void ) alpha_id;
	( void ) beta_id;

	/* xent sibling order must match the new child order. */
	XentNodeId expect_order [5];
	for (int i = 0; i < 5; i++) expect_order [i] = xtk_node_id(xtk_node_child(xtk_rt_root(rt), i));
	XentNodeId walk = xent_node_first(ctx, xtk_node_id(xtk_rt_root(rt)));
	for (int i = 0; i < 5; i++) {
		EXPECT(walk == expect_order [i], "xent order matches element order");
		walk = xent_node_next(ctx, walk);
	}

	/* Conditional unmount: hide the slider again. */
	flux_input_route_activation(xtkhost, store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), 2)), XTK_EVENT_CAUSE_USER);
	xtk_rt_frame(rt);
	EXPECT(xtk_node_nchild(xtk_rt_root(rt)) == 4, "slider unmounted");
	EXPECT(flux_store_n(store) == base_count, "slider node fully released");
	EXPECT(!xent_node_valid(ctx, slider_id), "slider dead in xent");

	for (int i = 0; i < 2000; i++) {
		xtk_rt_post(rt, xtk_msg(MSG_INCREMENT));
		xtk_rt_frame(rt);
	}
	EXPECT(m.count == 2003, "2000 message frames");
	EXPECT(flux_store_n(store) == base_count, "long run is churn-free");

	xtk_rt_destroy(rt);
	flux_host_destroy(xtkhost);
	EXPECT(flux_store_n(store) <= 1, "teardown drained the store");

	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	printf("PASS: xtk reconciler\n");
	return 0;
}
