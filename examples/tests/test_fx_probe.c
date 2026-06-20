/**
 * @file test_fx_probe.c
 * @brief Button geometry regression: label, icon+label, and icon-only buttons
 * keep their measured size inside hug rows (no squeeze-and-wrap slivers).
 *
 * Guards the trio of fixes: grow>0 implies flex-basis 0 (spacers stop
 * inflating the line), set_icon reserves glyph+gap padding, and icon-only
 * buttons get an explicit 40x32 footprint instead of a width-0 auto node.
 */
#include "flux_internal.h"

#include <math.h>
#include <stdio.h>

static void update(void *model, XtkMsg msg) {
	( void ) model;
	( void ) msg;
}

static XtkEl *view(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 20, .padding = {36, 28, 36, 28},
             .align = XENT_FLEX_ALIGN_STRETCH
    },
	  (XtkEl *[]) {
		xtk_row(ui, (XtkStackDesc) {.gap = 8}, (XtkEl *[]) {xtk_button(ui, "solo-plain", (XtkButtonDesc) {0}), XTK_END}),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8},
		  (XtkEl *[]) {xtk_button(ui, "solo-plain", (XtkButtonDesc) {.icon = "Save"}), XTK_END}
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8}, (XtkEl *[]) {xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Mail"}), XTK_END}
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8},
		  (XtkEl *[]) {
			xtk_button(ui, "trio-one", (XtkButtonDesc) {.icon = "Save"}),
			xtk_button(ui, "trio-two", (XtkButtonDesc) {.icon = "Share"}),
			xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Mail"}), XTK_END}
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8, .fill = true},
		  (XtkEl *[]) {
			xtk_button(ui, "pushed-left", (XtkButtonDesc) {0}), xtk_spacer(ui),
			xtk_button(ui, "pushed-right", (XtkButtonDesc) {0}), XTK_END}
		),
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

static XentRect node_rect(XentContext *ctx, XtkNode *n) {
	XentRect r = {0};
	xent_get_layout_rect(ctx, n->node, &r);
	return r;
}

int main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	xent_set_protocol(ctx, host, XENT_PROTOCOL_FLEX);

	int        m  = 0;
	FluxBackendCtx bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend be = flux_xtk_backend(&bctx);
	XtkRuntime *rt = xtk_runtime_create(ctx, &be, host, &m, update, view);
	if (rt) bctx.runtime = rt;
	EXPECT(rt, "runtime");

	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 1100.0f, 760.0f);

	XentRect plain     = node_rect(ctx, rt->root->children [0]->children [0]);
	XentRect iconlabel = node_rect(ctx, rt->root->children [1]->children [0]);
	XentRect icononly  = node_rect(ctx, rt->root->children [2]->children [0]);

	EXPECT(plain.h == 32.0f, "label button is one line tall");
	EXPECT(iconlabel.h == 32.0f, "icon+label button is one line tall");
	EXPECT(iconlabel.w > plain.w, "icon reserves extra width over the same label");
	EXPECT(icononly.w == 40.0f && icononly.h == 32.0f, "icon-only button has the 40x32 footprint");

	XtkNode *trio       = rt->root->children [3];
	float   prev_right = -1.0f;
	for (int i = 0; i < trio->child_count; i++) {
		XentRect r = node_rect(ctx, trio->children [i]);
		EXPECT(r.h == 32.0f, "trio buttons stay one line tall");
		EXPECT(r.x >= prev_right, "trio buttons do not overlap");
		prev_right = r.x + r.w;
	}

	XtkNode *fill  = rt->root->children [4];
	XentRect left  = node_rect(ctx, fill->children [0]);
	XentRect right = node_rect(ctx, fill->children [2]);
	XentRect row   = {0};
	xent_get_layout_rect(ctx, fill->node, &row);
	EXPECT(left.h == 32.0f && right.h == 32.0f, "fill-row buttons stay one line tall");
	EXPECT(right.x + right.w >= row.x + row.w - 1.0f, "spacer pushes the trailing button to the row edge");

	xtk_runtime_destroy(rt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: xtk button geometry\n");
	return 0;
}
