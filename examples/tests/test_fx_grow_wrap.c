/**
 * @file test_fx_grow_wrap.c
 * @brief Grow cards with wrapping text: the card's cross size must account for
 * text wrapped at the item's grow-distributed (used) main width, not its
 * single-line max-content. Guards the CSS Flexbox §9.4 algo-cross-item fix
 * (hypothetical cross measured at the used main size) in xent-core.
 *
 * Before the fix a grow card was sized to its 1-line height (text measured at
 * max-content width), then text wrapped taller at its narrow used width and
 * spilled out the bottom — the FluXent Gallery home cards.
 */
#include "flux_internal.h"
#include <stdio.h>

static void update(void *m, XtkMsg msg) { ( void ) m; ( void ) msg; }

static XtkEl *card(XtkUi *ui, char const *title, char const *body) {
	return xtk_grow(
	  xtk_card(
		ui, (XtkStackDesc) {.gap = 8},
		(XtkEl *[]) {
		  xtk_text(ui, title, (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, body, (XtkTextDesc) {.size = 13}), XTK_END}
	  ),
	  1.0f
	);
}

static XtkEl *view(XtkUi *ui, void *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
		  (XtkEl *[]) {
			card(ui, "Elm loop", "Controls post messages and the update function evolves the model while the view rebuilds the tree."),
			card(ui, "Keyed diff", "The reconciler diffs the element tree and patches retained controls keyed by identity across frames."),
			card(ui, "Plain C17", "Designated initializers and compound literals everywhere and zero is always the default for fields."),
			XTK_END}
		),
		XTK_END}
	);
}

static XentRect rr(XentContext *ctx, XtkNode *n) {
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
	xent_set_flex_direction(ctx, host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, host, XENT_FLEX_ALIGN_STRETCH);
	int        mm = 0;
	FluxBackendCtx bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend be = flux_xtk_backend(&bctx);
	XtkRuntime *rt = xtk_runtime_create(ctx, &be, host, &mm, update, view);
	if (rt) bctx.runtime = rt;
	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 380.0f, 760.0f); /* narrow → cards ~112px → text wraps */

	XtkNode *row = rt->root->children [0];
	int     bad = 0;
	for (int i = 0; i < row->child_count; i++) {
		XtkNode  *c  = row->children [i];
		XentRect cr = rr(ctx, c);
		XtkNode  *tx = c->children [c->child_count - 1];
		XentRect tr = rr(ctx, tx);
		int      overflow = (tr.y + tr.h > cr.y + cr.h + 0.5f);
		if (overflow) bad++;
		printf(
		  "card%d w=%.0f h=%.0f bottom=%.0f | text h=%.0f bottom=%.0f %s\n", i, cr.w, cr.h, cr.y + cr.h, tr.h,
		  tr.y + tr.h, overflow ? "OVERFLOW" : "ok"
		);
	}
	printf("%s\n", bad ? "FAIL: text overflows card" : "PASS: text contained in card");

	xtk_runtime_destroy(rt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	return bad ? 1 : 0;
}
