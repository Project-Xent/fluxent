/**
 * @file test_fx_list.c
 * @brief Headless ListView probe against the real fluxent backend: spine
 * shape (root/scroll/ABSOLUTE host), content extent, realized window vs
 * viewport, row geometry, scroll-driven staleness → re-realization, and
 * node recycling on a same-size window shift.
 */
#include "flux_internal.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_host.h>

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

static void   list_stale_for_test(void *ctx) { xtk_rt_invalidate(( XtkRt * ) ctx); }

static XtkEl *row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "row %d", index), (XtkTextDesc) {0});
}

static XtkEl *view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_sized(
	  xtk_list(
		ui,
		(XtkListDesc) {
		  .nitem  = ROWS,
		  .itemh  = 40.0f,
		  .item   = row,
		  .env    = m,
		  .sel    = m->selected,
		  .select = xtk_msg(MSG_SELECT),
		}
	  ),
	  800.0f, 600.0f
	);
}

static XtkEl *grid_view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_sized(
	  xtk_gridview(
		ui,
		(XtkListDesc) {
		  .nitem  = 100,
		  .itemh  = 100.0f,
		  .itemw  = 200.0f,
		  .item   = row,
		  .env    = m,
		  .sel    = m->selected,
		  .select = xtk_msg(MSG_SELECT),
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
	XentCfg    config = {0};
	XentCtx   *ctx    = xent_ctx_create(&config);
	FluxStore *store  = flux_store_create(64);
	flux_store_bind(store, ctx);

	/* Generation-safe sidecar: destroy + slot reuse must not resurrect old data. */
	{
		XentNodeId    a  = xent_node_create(ctx);
		FluxNodeData *na = flux_store_ensure(store, a);
		EXPECT(na, "sidecar create");
		na->component_type = FLUX_CONTROL_BUTTON;
		XentNodeId stale   = a;
		EXPECT(xent_node_destroy(ctx, a), "destroy for reuse");
		EXPECT(!flux_store_find(store, stale), "observer cleared stale sidecar");
		EXPECT(!flux_store_ensure(store, stale), "stale get_or_create rejected");
		flux_node_settype(store, stale, FLUX_CONTROL_BUTTON);
		EXPECT(!flux_store_find(store, stale), "stale set_control_type leaves no ghost");
		XentNodeId b = xent_node_create(ctx);
		EXPECT(xent_node_index(b) == xent_node_index(stale), "slot reused");
		EXPECT(b != stale, "generation differs");
		FluxNodeData *nb = flux_store_ensure(store, b);
		EXPECT(nb, "fresh sidecar for new handle");
		EXPECT(nb->component_type == FLUX_CONTROL_CONTAINER, "reused slot has default type");
		flux_node_settype(store, b, FLUX_CONTROL_SCROLL);
		EXPECT(flux_node_type(store, b) == FLUX_CONTROL_SCROLL, "store-typed control");
		EXPECT(!flux_store_find(store, stale), "stale handle still absent");
		EXPECT(!flux_store_ensure(store, stale), "stale create still rejected after reuse");
		xent_node_destroy(ctx, b);
	}

	XentNodeId host = xent_node_create(ctx);
	xent_setproto(ctx, host, XENT_PROTOCOL_FLEX);

	Model          m       = {.selected = -1};
	FluxBackendCtx bctx    = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkHost       *xtkhost = flux_host_adopt(ctx, store, host);
	EXPECT(xtkhost, "host adopt");
	bctx.host    = xtkhost;
	XtkRtCfg cfg = flux_xtk_runtime_config(&bctx);
	XtkRt   *rt  = xtk_rt_create(xtkhost, &cfg, host, &m, update, view);
	EXPECT(rt, "runtime");
	bctx.runtime = rt;

	xtk_rt_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);

	XentNodeId    list = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *lnd  = flux_store_find(store, list);
	EXPECT(lnd && lnd->component_type == FLUX_CONTROL_LIST && lnd->component_data, "list data attached");
	FluxListViewData *ld = ( FluxListViewData * ) lnd->component_data;
	/* First-mount list_realized sees an invalid id, so wire staleness here. */
	flux_list_onstale(store, list, list_stale_for_test, rt);

	/* Spine + extent: the logical extent lives on the scroll (manual +
	 * virtualized); the host is a small rebased canvas spanning just the
	 * realized window, so physical coordinates never reach 4e6. */
	EXPECT(ld->scroll != XENT_NODE_INVALID && ld->host != XENT_NODE_INVALID, "scroll + host exist");
	FluxNodeData   *snd = flux_store_find(store, ld->scroll);
	FluxScrollData *sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(sd->content_manual && sd->virtualized, "virtual extent is manual + rebased");
	EXPECT(sd->content_h == ( float ) ROWS * 40.0f, "logical extent = count * itemh");
	XentRect host_rect = {0};
	xent_layout_rect(ctx, ld->host, &host_rect);
	EXPECT(host_rect.h == 19.0f * 40.0f, "items host spans the realized window only");
	EXPECT(host_rect.w == 800.0f, "items host fills the list width");

	/* Mount realized the fallback window (list height 600 → rows 0..18). */
	EXPECT(ld->realized_first == 0 && ld->realized_last == 18, "mount realizes 0..18");
	EXPECT(xtk_node_nchild(xtk_rt_root(rt)) == 19, "19 retained rows");

	/* Row geometry: absolute slots, full width. */
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		XentRect r = {0};
		xent_layout_rect(ctx, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)), &r);
		EXPECT(r.y == host_rect.y + ( float ) i * 40.0f, "row sits in its index slot");
		EXPECT(r.h == 40.0f, "row is itemh tall");
		EXPECT(r.w == 800.0f, "row fills the list width");
	}

	/* Viewport covered → the engine watch stays quiet. */
	if (flux_list_window_stale(ctx, lnd)) flux_list_notify_stale(store, list);
	EXPECT(xtk_rt_frame(rt).status == XTK_FRAME_CLEAN, "covered viewport does not invalidate");

	/* Scroll far outside the window: watch fires exactly once, the next
	 * frame re-realizes around the new offset. */
	snd          = flux_store_find(store, ld->scroll);
	sd           = ( FluxScrollData * ) snd->component_data;
	sd->scroll_y = 4000.0f;
	if (flux_list_window_stale(ctx, lnd)) flux_list_notify_stale(store, list);
	if (flux_list_window_stale(ctx, lnd)) flux_list_notify_stale(store, list);

	EXPECT(xtk_rt_frame(rt).status == XTK_FRAME_COMMITTED, "uncovered viewport invalidates the runtime");
	xent_layout(ctx, host, 800.0f, 600.0f);
	lnd = flux_store_find(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	/* offset 4000, extent 600: first floor(100)-4 = 96, last ceil(4600/40)-1+4 = 118. */
	EXPECT(ld->realized_first == 96 && ld->realized_last == 118, "window re-realizes to 96..118");

	/* Rebase followed the window: origin = slot 96, rows sit near the host top. */
	snd = flux_store_find(store, ld->scroll);
	sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(sd->origin_y == 96.0f * 40.0f, "origin rebased to the first realized row");
	xent_layout_rect(ctx, ld->host, &host_rect);
	EXPECT(host_rect.h == 23.0f * 40.0f, "host tracks the realized span");
	XentRect first_rect = {0};
	xent_layout_rect(ctx, xtk_node_id(xtk_node_child(xtk_rt_root(rt), 0)), &first_rect);
	EXPECT(first_rect.y == host_rect.y, "first realized row rebased to the host origin");
	/* Screen math is unchanged: physical − residual == logical − scroll. */
	EXPECT(
	  first_rect.y - flux_scroll_off_y(sd) == host_rect.y + 96.0f * 40.0f - sd->scroll_y,
	  "rebased screen position matches the logical position"
	);

	/* Same-size shift: every xent node survives (pure recycling). */
	XentNodeId before [64];
	int        n_before = xtk_node_nchild(xtk_rt_root(rt));
	for (int i = 0; i < n_before; i++) before [i] = xtk_node_id(xtk_node_child(xtk_rt_root(rt), i));

	sd->scroll_y = 4400.0f;
	if (flux_list_window_stale(ctx, lnd)) flux_list_notify_stale(store, list);
	EXPECT(xtk_rt_frame(rt).status == XTK_FRAME_COMMITTED, "shifted viewport invalidates again");
	EXPECT(ld->realized_first == 106 && ld->realized_last == 128, "window shifts to 106..128");
	EXPECT(xtk_node_nchild(xtk_rt_root(rt)) == n_before, "window size unchanged");
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		bool found = false;
		for (int j = 0; j < n_before; j++)
			if (before [j] == xtk_node_id(xtk_node_child(xtk_rt_root(rt), i))) found = true;
		EXPECT(found, "same-size shift recycles every node");
	}

	/* Selection flows through the recycled rows as a prop patch. */
	xtk_rt_post(rt, xtk_msg_i(MSG_SELECT, 110));
	xtk_rt_frame(rt);
	EXPECT(m.selected == 110, "selection message consumed");
	FluxNodeData *row110 = NULL;
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 110 && it->selected) row110 = ind;
	}
	EXPECT(row110, "row 110 carries the selected visual");

	/* Keyboard: Down from row 110 selects 111 (Single, selection follows
	 * focus); Home jumps to row 0 (scroll-into-view realizes the top). */
	EXPECT(row110->behavior.on_key, "rows carry the key handler");
	row110->behavior.on_key(row110->behavior.on_key_ctx, FLUX_KEY_DOWN, true);
	xtk_rt_frame(rt);
	EXPECT(m.selected == 111, "Down selects the next row (follows focus)");

	FluxNodeData *row111 = NULL;
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 111) row111 = ind;
	}
	EXPECT(row111, "row 111 realized");
	row111->behavior.on_key(row111->behavior.on_key_ctx, FLUX_KEY_HOME, true);
	xtk_rt_frame(rt);
	xent_layout(ctx, host, 800.0f, 600.0f);
	lnd = flux_store_find(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	EXPECT(m.selected == 0, "Home selects row 0");
	EXPECT(ld->realized_first == 0, "Home scrolled the window back to the top");

	/* Click gesture routes through the same state machine. */
	FluxNodeData *row0 = NULL;
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) row0 = ind;
	}
	EXPECT(row0 && row0->behavior.on_click, "row 5 realized with click handler");
	row0->behavior.on_click(row0->behavior.on_click_ctx);
	xtk_rt_frame(rt);
	EXPECT(m.selected == 5, "click selects row 5");

	/* Deep scroll (row 99000, logical y ≈ 3.96e6): the rebase keeps every
	 * physical coordinate viewport-sized while row pitch stays exact (integer
	 * multiples of 40 are exact in float32 below 2^24). This is the guarantee
	 * that kills the compositor float-precision drift. */
	snd          = flux_store_find(store, ld->scroll);
	sd           = ( FluxScrollData * ) snd->component_data;
	sd->scroll_y = 99000.0f * 40.0f;
	if (flux_list_window_stale(ctx, lnd)) flux_list_notify_stale(store, list);
	EXPECT(xtk_rt_frame(rt).status == XTK_FRAME_COMMITTED, "deep offset invalidates the runtime");
	xent_layout(ctx, host, 800.0f, 600.0f);
	lnd = flux_store_find(store, list);
	ld  = ( FluxListViewData * ) lnd->component_data;
	snd = flux_store_find(store, ld->scroll);
	sd  = ( FluxScrollData * ) snd->component_data;
	EXPECT(ld->realized_first == 98996 && ld->realized_last == 99018, "deep window 98996..99018");
	EXPECT(sd->origin_y == 98996.0f * 40.0f, "origin rebased to the deep window");
	EXPECT(sd->content_h == ( float ) ROWS * 40.0f, "logical extent intact for the scrollbar");
	xent_layout_rect(ctx, ld->host, &host_rect);
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(rt)); i++) {
		XentRect r = {0};
		xent_layout_rect(ctx, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)), &r);
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(rt), i)));
		FluxListItemData *it  = ( FluxListItemData * ) ind->component_data;
		EXPECT(r.y - host_rect.y >= 0.0f && r.y - host_rect.y <= 22.0f * 40.0f, "physical slot stays viewport-sized");
		EXPECT(
		  r.y - flux_scroll_off_y(sd) == host_rect.y + ( float ) it->index * 40.0f - sd->scroll_y,
		  "deep rebased screen position is exact"
		);
	}

	xtk_rt_destroy(rt);
	flux_host_destroy(xtkhost);

	/* ----------------------------------------------------------------
	 * GridView: auto columns from viewport width + 2D keyboard nav.
	 * -------------------------------------------------------------- */
	XentNodeId ghost = xent_node_create(ctx);
	xent_setproto(ctx, ghost, XENT_PROTOCOL_FLEX);
	m.selected          = -1;
	XtkHost *ghost_host = flux_host_adopt(ctx, store, ghost);
	EXPECT(ghost_host, "ghost host adopt");
	bctx.host  = ghost_host;
	XtkRt *grt = xtk_rt_create(ghost_host, &cfg, ghost, &m, update, grid_view);
	EXPECT(grt, "grid runtime");
	bctx.runtime = grt;

	xtk_rt_frame(grt);
	xent_layout(ctx, ghost, 800.0f, 600.0f);
	xtk_rt_invalidate(grt); /* first realize used the pre-layout fallback */
	xtk_rt_frame(grt);
	xent_layout(ctx, ghost, 800.0f, 600.0f);

	FluxNodeData     *gnd = flux_store_find(store, xtk_node_id(xtk_rt_root(grt)));
	FluxListViewData *gld = ( FluxListViewData * ) gnd->component_data;
	EXPECT(gld->kind == XTK_LIST_KIND_GRID, "grid kind");
	EXPECT(gld->cols == 4, "800px viewport / 200px cells = 4 columns");
	/* Viewport 600/100 = rows 0..5 visible, +4 overscan → rows 0..9 → cells 0..39. */
	EXPECT(gld->realized_first == 0 && gld->realized_last == 39, "grid window 0..39");

	XentRect c5 = {0};
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(grt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(grt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) xent_layout_rect(ctx, xtk_node_id(xtk_node_child(xtk_rt_root(grt), i)), &c5);
	}
	EXPECT(c5.w == 200.0f && c5.h == 100.0f, "cell 5 has the grid cell size");
	EXPECT(c5.x == 200.0f && c5.y == 100.0f, "cell 5 sits at column 1, row 1");

	/* 2D nav: Down from cell 5 = cell 9 (one row down). */
	FluxNodeData *cell5 = NULL;
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(grt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(grt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 5) cell5 = ind;
	}
	EXPECT(cell5, "cell 5 realized");
	cell5->behavior.on_key(cell5->behavior.on_key_ctx, FLUX_KEY_DOWN, true);
	xtk_rt_frame(grt);
	EXPECT(m.selected == 9, "grid Down moves one row (+cols)");
	cell5 = NULL;
	for (int i = 0; i < xtk_node_nchild(xtk_rt_root(grt)); i++) {
		FluxNodeData     *ind = flux_store_find(store, xtk_node_id(xtk_node_child(xtk_rt_root(grt), i)));
		FluxListItemData *it  = ind ? ( FluxListItemData * ) ind->component_data : NULL;
		if (it && it->index == 9) cell5 = ind;
	}
	EXPECT(cell5, "cell 9 realized");
	cell5->behavior.on_key(cell5->behavior.on_key_ctx, FLUX_KEY_RIGHT, true);
	xtk_rt_frame(grt);
	EXPECT(m.selected == 10, "grid Right moves one column (+1)");

	xtk_rt_destroy(grt);
	flux_host_destroy(ghost_host);

	/* Extended and Multiple share retained multi-selection semantics, but only
	 * Multiple owns the inline checkbox chrome. */
	XentNodeId chrome_root = xent_node_create(ctx);
	XentNodeId chrome_list = flux_list_init(&(FluxListViewDesc) {
	  .ctx = ctx, .store = store, .parent = chrome_root, .kind = XTK_LIST_KIND_LIST});
	XentNodeId chrome_host = flux_list_content_node(store, chrome_list);
	XentNodeId chrome_item = flux_listitem_init(&(FluxListItemDesc) {ctx, store, chrome_host});
	EXPECT(chrome_item != XENT_NODE_INVALID, "selection chrome item");
	flux_list_set_extent(store, chrome_list, 1, 40.0f, 0.0f, 1);
	flux_list_setmode(store, chrome_list, XTK_LIST_SELECT_EXTENDED);
	flux_list_set_realized(store, chrome_list, 0, 0, 1);
	FluxNodeData     *chrome_nd = flux_store_find(store, chrome_item);
	FluxListItemData *chrome    = chrome_nd ? ( FluxListItemData * ) chrome_nd->component_data : NULL;
	EXPECT(chrome && !chrome->multi, "Extended mode has no checkbox chrome");
	flux_list_setmode(store, chrome_list, XTK_LIST_SELECT_MULTIPLE);
	flux_list_apply_pending(store, chrome_list);
	EXPECT(chrome->multi, "Multiple mode has checkbox chrome");

	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	printf("PASS: fluxent virtualized list\n");
	return 0;
}
