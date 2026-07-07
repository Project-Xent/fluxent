/**
 * @file test_fx_list.c
 * @brief Headless ListView probe against the real fluxent backend: spine
 * shape (root/scroll/ABSOLUTE host), content extent, realized window vs
 * viewport, row geometry, scroll-driven staleness → re-realization, and
 * node recycling on a same-size window shift.
 */
#include "flux_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define ROWS 100000

enum
{
	MSG_NONE = 0,
	MSG_SELECT,
};

typedef struct Model {
	int selected;
} Model;

static void update(void *model, XtkMsg msg) {
	Model *m = ( Model * ) model;
	if (msg.tag == MSG_SELECT) m->selected = msg.i;
}

static XtkEl *row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "row %d", index), (XtkTextDesc) {0});
}

static XtkEl *view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_sized(
	  xtk_list_view(
	    ui, (XtkListDesc) {
	          .count       = ROWS,
	          .item_height = 40.0f,
	          .item        = row,
	          .env         = m,
	          .selected    = m->selected,
	          .on_select   = xtk_msg(MSG_SELECT),
	        }
	  ),
	  800.0f, 600.0f
	);
}

static XtkEl *grid_view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_sized(
	  xtk_grid_view(
	    ui, (XtkListDesc) {
	          .count       = 100,
	          .item_height = 100.0f,
	          .item_width  = 200.0f,
	          .item        = row,
	          .env         = m,
	          .selected    = m->selected,
	          .on_select   = xtk_msg(MSG_SELECT),
	        }
	  ),
	  800.0f, 600.0f
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

int main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	xent_set_protocol(ctx, host, XENT_PROTOCOL_FLEX);

	Model           m    = {.selected = -1};
	FluxBackendCtx  bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend      be   = flux_xtk_backend(&bctx);
	XtkRuntime     *rt   = xtk_runtime_create(ctx, &be, host, &m, update, view);
	EXPECT(rt, "runtime");
	bctx.runtime = rt;

	/* Mirror the app frame: layout, then re-attach userdata (the node store
	 * rehashes as nodes are added, moving FluxNodeData; the app does this
	 * after every layout, before the engine collect pass). */
	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);

	XentNodeId        list = rt->root->node;
	FluxNodeData     *lnd  = flux_node_store_get(store, list);
	EXPECT(lnd && lnd->component_type == FLUX_CONTROL_LIST && lnd->component_data, "list data attached");
	FluxListViewData *ld = ( FluxListViewData * ) lnd->component_data;

	/* Spine + extent: the logical extent lives on the scroll (manual +
	 * virtualized); the host is a small rebased canvas spanning just the
	 * realized window, so physical coordinates never reach 4e6. */
	EXPECT(ld->scroll != XENT_NODE_INVALID && ld->host != XENT_NODE_INVALID, "scroll + host exist");
	FluxNodeData   *snd = flux_node_store_get(store, ld->scroll);
	FluxScrollData *sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(sd->content_manual && sd->virtualized, "virtual extent is manual + rebased");
	EXPECT(sd->content_h == ( float ) ROWS * 40.0f, "logical extent = count * item_height");
	XentRect host_rect = {0};
	xent_get_layout_rect(ctx, ld->host, &host_rect);
	EXPECT(host_rect.h == 19.0f * 40.0f, "items host spans the realized window only");
	EXPECT(host_rect.w == 800.0f, "items host fills the list width");

	/* Mount realized the fallback window (list height 600 → rows 0..18). */
	EXPECT(ld->realized_first == 0 && ld->realized_last == 18, "mount realizes 0..18");
	EXPECT(rt->root->child_count == 19, "19 retained rows");

	/* Row geometry: absolute slots, full width. */
	for (int i = 0; i < rt->root->child_count; i++) {
		XentRect r = {0};
		xent_get_layout_rect(ctx, rt->root->children [i]->node, &r);
		EXPECT(r.y == host_rect.y + ( float ) i * 40.0f, "row sits in its index slot");
		EXPECT(r.h == 40.0f, "row is item_height tall");
		EXPECT(r.w == 800.0f, "row fills the list width");
	}

	/* Viewport covered → the engine watch stays quiet. */
	flux_list_view_update_window(ctx, list, lnd);
	EXPECT(!rt->force, "covered viewport does not invalidate");

	/* Scroll far outside the window: watch fires exactly once, the next
	 * frame re-realizes around the new offset. */
	snd          = flux_node_store_get(store, ld->scroll);
	sd           = ( FluxScrollData * ) snd->component_data;
	sd->scroll_y = 4000.0f;
	flux_list_view_update_window(ctx, list, lnd);
	EXPECT(rt->force, "uncovered viewport invalidates the runtime");
	flux_list_view_update_window(ctx, list, lnd);

	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);
	lnd = flux_node_store_get(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	/* offset 4000, extent 600: first floor(100)-4 = 96, last ceil(4600/40)-1+4 = 118. */
	EXPECT(ld->realized_first == 96 && ld->realized_last == 118, "window re-realizes to 96..118");

	/* Rebase followed the window: origin = slot 96, rows sit near the host top. */
	snd = flux_node_store_get(store, ld->scroll);
	sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(sd->origin_y == 96.0f * 40.0f, "origin rebased to the first realized row");
	xent_get_layout_rect(ctx, ld->host, &host_rect);
	EXPECT(host_rect.h == 23.0f * 40.0f, "host tracks the realized span");
	XentRect first_rect = {0};
	xent_get_layout_rect(ctx, rt->root->children [0]->node, &first_rect);
	EXPECT(first_rect.y == host_rect.y, "first realized row rebased to the host origin");
	/* Screen math is unchanged: physical − residual == logical − scroll. */
	EXPECT(
	  first_rect.y - flux_scroll_off_y(sd) == host_rect.y + 96.0f * 40.0f - sd->scroll_y,
	  "rebased screen position matches the logical position"
	);

	/* Same-size shift: every xent node survives (pure recycling). */
	XentNodeId before [64];
	int        n_before = rt->root->child_count;
	for (int i = 0; i < n_before; i++) before [i] = rt->root->children [i]->node;

	sd->scroll_y = 4400.0f;
	flux_list_view_update_window(ctx, list, lnd);
	EXPECT(rt->force, "shifted viewport invalidates again");
	xtk_runtime_frame(rt);
	EXPECT(ld->realized_first == 106 && ld->realized_last == 128, "window shifts to 106..128");
	EXPECT(rt->root->child_count == n_before, "window size unchanged");
	for (int i = 0; i < rt->root->child_count; i++) {
		bool found = false;
		for (int j = 0; j < n_before; j++)
			if (before [j] == rt->root->children [i]->node) found = true;
		EXPECT(found, "same-size shift recycles every node");
	}

	/* Selection flows through the recycled rows as a prop patch. */
	xtk_runtime_post(rt, xtk_msg_i(MSG_SELECT, 110));
	xtk_runtime_frame(rt);
	EXPECT(m.selected == 110, "selection message consumed");
	FluxNodeData *row110 = NULL;
	for (int i = 0; i < rt->root->child_count; i++) {
		FluxNodeData *ind = flux_node_store_get(store, rt->root->children [i]->node);
		FluxListItemData *it = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 110 && it->selected) row110 = ind;
	}
	EXPECT(row110, "row 110 carries the selected visual");

	/* Keyboard: Down from row 110 selects 111 (Single, selection follows
	 * focus); Home jumps to row 0 (scroll-into-view realizes the top). */
	EXPECT(row110->behavior.on_key, "rows carry the key handler");
	row110->behavior.on_key(row110->behavior.on_key_ctx, VK_DOWN, true);
	xtk_runtime_frame(rt);
	EXPECT(m.selected == 111, "Down selects the next row (follows focus)");

	FluxNodeData *row111 = NULL;
	for (int i = 0; i < rt->root->child_count; i++) {
		FluxNodeData     *ind = flux_node_store_get(store, rt->root->children [i]->node);
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 111) row111 = ind;
	}
	EXPECT(row111, "row 111 realized");
	row111->behavior.on_key(row111->behavior.on_key_ctx, VK_HOME, true);
	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);
	lnd = flux_node_store_get(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	EXPECT(m.selected == 0, "Home selects row 0");
	EXPECT(ld->realized_first == 0, "Home scrolled the window back to the top");

	/* Click gesture routes through the same state machine. */
	FluxNodeData *row0 = NULL;
	for (int i = 0; i < rt->root->child_count; i++) {
		FluxNodeData     *ind = flux_node_store_get(store, rt->root->children [i]->node);
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) row0 = ind;
	}
	EXPECT(row0 && row0->behavior.on_click, "row 5 realized with click handler");
	row0->behavior.on_click(row0->behavior.on_click_ctx);
	xtk_runtime_frame(rt);
	EXPECT(m.selected == 5, "click selects row 5");

	/* Deep scroll (row 99000, logical y ≈ 3.96e6): the rebase keeps every
	 * physical coordinate viewport-sized while row pitch stays exact (integer
	 * multiples of 40 are exact in float32 below 2^24). This is the guarantee
	 * that kills the compositor float-precision drift. */
	snd          = flux_node_store_get(store, ld->scroll);
	sd           = ( FluxScrollData * ) snd->component_data;
	sd->scroll_y = 99000.0f * 40.0f;
	flux_list_view_update_window(ctx, list, lnd);
	EXPECT(rt->force, "deep offset invalidates the runtime");
	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);
	lnd = flux_node_store_get(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	snd = flux_node_store_get(store, ld->scroll);
	sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(ld->realized_first == 98996 && ld->realized_last == 99018, "deep window 98996..99018");
	EXPECT(sd->origin_y == 98996.0f * 40.0f, "origin rebased to the deep window");
	EXPECT(sd->content_h == ( float ) ROWS * 40.0f, "logical extent intact for the scrollbar");
	xent_get_layout_rect(ctx, ld->host, &host_rect);
	for (int i = 0; i < rt->root->child_count; i++) {
		XentRect r = {0};
		xent_get_layout_rect(ctx, rt->root->children [i]->node, &r);
		FluxNodeData     *ind = flux_node_store_get(store, rt->root->children [i]->node);
		FluxListItemData *it  = ( FluxListItemData * ) ind->component_data;
		EXPECT(r.y - host_rect.y >= 0.0f && r.y - host_rect.y <= 22.0f * 40.0f, "physical slot stays viewport-sized");
		EXPECT(
		  r.y - flux_scroll_off_y(sd) == host_rect.y + ( float ) it->index * 40.0f - sd->scroll_y,
		  "deep rebased screen position is exact"
		);
	}

	xtk_runtime_destroy(rt);

	/* ----------------------------------------------------------------
	 * GridView: auto columns from viewport width + 2D keyboard nav.
	 * -------------------------------------------------------------- */
	XentNodeId ghost = xent_create_node(ctx);
	xent_set_protocol(ctx, ghost, XENT_PROTOCOL_FLEX);
	m.selected      = -1;
	XtkRuntime *grt = xtk_runtime_create(ctx, &be, ghost, &m, update, grid_view);
	EXPECT(grt, "grid runtime");
	bctx.runtime = grt;

	xtk_runtime_frame(grt);
	xent_layout(ctx, ghost, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);
	xtk_runtime_invalidate(grt); /* first realize used the pre-layout fallback */
	xtk_runtime_frame(grt);
	xent_layout(ctx, ghost, 800.0f, 600.0f);
	flux_node_store_attach_userdata(store, ctx);

	FluxNodeData     *gnd = flux_node_store_get(store, grt->root->node);
	FluxListViewData *gld = ( FluxListViewData * ) gnd->component_data;
	EXPECT(gld->kind == XTK_LIST_KIND_GRID, "grid kind");
	EXPECT(gld->cols == 4, "800px viewport / 200px cells = 4 columns");
	/* Viewport 600/100 = rows 0..5 visible, +4 overscan → rows 0..9 → cells 0..39. */
	EXPECT(gld->realized_first == 0 && gld->realized_last == 39, "grid window 0..39");

	XentRect c5 = {0};
	for (int i = 0; i < grt->root->child_count; i++) {
		FluxNodeData     *ind = flux_node_store_get(store, grt->root->children [i]->node);
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) xent_get_layout_rect(ctx, grt->root->children [i]->node, &c5);
	}
	EXPECT(c5.w == 200.0f && c5.h == 100.0f, "cell 5 has the grid cell size");
	EXPECT(c5.x == 200.0f && c5.y == 100.0f, "cell 5 sits at column 1, row 1");

	/* 2D nav: Down from cell 5 = cell 9 (one row down). */
	FluxNodeData *cell5 = NULL;
	for (int i = 0; i < grt->root->child_count; i++) {
		FluxNodeData     *ind = flux_node_store_get(store, grt->root->children [i]->node);
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) cell5 = ind;
	}
	EXPECT(cell5, "cell 5 realized");
	cell5->behavior.on_key(cell5->behavior.on_key_ctx, VK_DOWN, true);
	xtk_runtime_frame(grt);
	EXPECT(m.selected == 9, "grid Down moves one row (+cols)");
	cell5 = NULL;
	for (int i = 0; i < grt->root->child_count; i++) {
		FluxNodeData     *ind = flux_node_store_get(store, grt->root->children [i]->node);
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 9) cell5 = ind;
	}
	EXPECT(cell5, "cell 9 realized");
	cell5->behavior.on_key(cell5->behavior.on_key_ctx, VK_RIGHT, true);
	xtk_runtime_frame(grt);
	EXPECT(m.selected == 10, "grid Right moves one column (+1)");

	xtk_runtime_destroy(grt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: fluxent virtualized list\n");
	return 0;
}
