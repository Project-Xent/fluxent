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

#include <fluxent/flux_host.h>

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
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8}, (XtkEl *[]) {xtk_button(ui, "solo-plain", (XtkButtonDesc) {0}), XTK_END}
		),
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

static XentRect node_rect(XentCtx *ctx, XtkNode const *n) {
	XentRect r = {0};
	xent_layout_rect(ctx, xtk_node_id(n), &r);
	return r;
}

int main(void) {
	XentCfg    config = {0};
	XentCtx   *ctx    = xent_ctx_create(&config);
	FluxStore *store  = flux_store_create(64);
	flux_store_bind(store, ctx);

	XentNodeId host = xent_node_create(ctx);
	xent_setproto(ctx, host, XENT_PROTOCOL_FLEX);

	int            m       = 0;
	FluxBackendCtx bctx    = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkHost       *xtkhost = flux_host_adopt(ctx, store, host);
	EXPECT(xtkhost, "host adopt");
	bctx.host    = xtkhost;
	XtkRtCfg cfg = flux_xtk_runtime_config(&bctx);
	XtkRt   *rt  = xtk_rt_create(xtkhost, &cfg, host, &m, update, view);
	if (rt) bctx.runtime = rt;
	EXPECT(rt, "runtime");

	xtk_rt_frame(rt);
	xent_layout(ctx, host, 1100.0f, 760.0f);

	XentRect plain     = node_rect(ctx, xtk_node_child(xtk_node_child(xtk_rt_root(rt), 0), 0));
	XentRect iconlabel = node_rect(ctx, xtk_node_child(xtk_node_child(xtk_rt_root(rt), 1), 0));
	XentRect icononly  = node_rect(ctx, xtk_node_child(xtk_node_child(xtk_rt_root(rt), 2), 0));

	EXPECT(plain.h == 32.0f, "label button is one line tall");
	EXPECT(iconlabel.h == 32.0f, "icon+label button is one line tall");
	EXPECT(iconlabel.w > plain.w, "icon reserves extra width over the same label");
	EXPECT(icononly.w == 40.0f && icononly.h == 32.0f, "icon-only button has the 40x32 footprint");

	XtkNode const *trio       = xtk_node_child(xtk_rt_root(rt), 3);
	float          prev_right = -1.0f;
	for (int i = 0; i < xtk_node_nchild(trio); i++) {
		XentRect r = node_rect(ctx, xtk_node_child(trio, i));
		EXPECT(r.h == 32.0f, "trio buttons stay one line tall");
		EXPECT(r.x >= prev_right, "trio buttons do not overlap");
		prev_right = r.x + r.w;
	}

	XtkNode const *fill  = xtk_node_child(xtk_rt_root(rt), 4);
	XentRect       left  = node_rect(ctx, xtk_node_child(fill, 0));
	XentRect       right = node_rect(ctx, xtk_node_child(fill, 2));
	XentRect       row   = {0};
	xent_layout_rect(ctx, xtk_node_id(fill), &row);
	EXPECT(left.h == 32.0f && right.h == 32.0f, "fill-row buttons stay one line tall");
	EXPECT(right.x + right.w >= row.x + row.w - 1.0f, "spacer pushes the trailing button to the row edge");

	xtk_rt_destroy(rt);
	flux_host_destroy(xtkhost);
	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	printf("PASS: xtk button geometry\n");
	return 0;
}
