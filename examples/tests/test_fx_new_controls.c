/**
 * @file test_fx_new_controls.c
 * @brief Headless mount/layout probes for the nine new WinUI 3 controls:
 * RadioButtons, RatingControl, ToggleSplitButton, BreadcrumbBar, SelectorBar,
 * TeachingTip, TreeView, ItemsView, and PullToRefresh.
 */
#include "flux_internal.h"

#include "fluxent/controls/flux_list_view_data.h"
#include "fluxent/controls/flux_pager_data.h"
#include "fluxent/controls/flux_person_picture_data.h"
#include "fluxent/controls/flux_radio_buttons_data.h"
#include "fluxent/controls/flux_split_view_data.h"
#include "fluxent/controls/flux_title_bar_data.h"
#include "fluxent/controls/flux_refresh_data.h"
#include "fluxent/controls/flux_tree_view_data.h"

#include <string.h>

#include <math.h>
#include <stdio.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static void noop_update(void *model, XtkMsg msg) {
	( void ) model;
	( void ) msg;
}

static XentRect node_rect(XentContext *ctx, XtkNode *n) {
	XentRect r = {0};
	if (n) xent_get_layout_rect(ctx, n->node, &r);
	return r;
}

static int run_view(
  char const *name, void *model, XtkEl *(*build)(XtkUi *, void *),
  int (*check)(XentContext *, FluxNodeStore *, XtkRuntime *)
) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(128);
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	xent_set_protocol(ctx, host, XENT_PROTOCOL_FLEX);

	FluxBackendCtx bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend     be   = flux_xtk_backend(&bctx);
	XtkRuntime    *rt   = xtk_runtime_create(ctx, &be, host, model, noop_update, build);
	EXPECT(rt, "runtime");
	bctx.runtime = rt;

	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 900.0f, 640.0f);
	flux_node_store_attach_userdata(store, ctx);

	int rc = check(ctx, store, rt);
	xtk_runtime_destroy(rt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	if (rc != 0) return rc;
	printf("PASS: %s\n", name);
	return 0;
}

/* ---------------------------------------------------------------- RadioButtons */

static XtkRadioItem const kRb [] = {
  {.label = "A"},
  {.label = "B"},
  {.label = "C"},
};

static XtkEl *view_radio_buttons(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_radio_buttons(
	  ui, (XtkRadioButtonsDesc) {
		.header     = "Group",
		.items      = kRb,
		.item_count = 3,
		.selected   = 1,
	      }
	);
}

static int check_radio_buttons(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_RADIO_BUTTONS, "radio buttons data");
	FluxRadioButtonsData *g = ( FluxRadioButtonsData * ) nd->component_data;
	EXPECT(g && g->count == 3, "three radio items");
	XentRect r = node_rect(ctx, rt->root);
	EXPECT(r.h >= 96.0f, "radio group has vertical extent");
	return 0;
}

/* ---------------------------------------------------------------- Rating */

static XtkEl *view_rating(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_rating(ui, (XtkRatingDesc) {.value = 3.0, .max_rating = 5, .caption = "Stars"});
}

static int check_rating(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) store;
	XentRect r = node_rect(ctx, rt->root);
	EXPECT(r.w >= 120.0f && r.h >= 28.0f, "rating strip has size");
	return 0;
}

/* ---------------------------------------------------------------- ToggleSplitButton */

static XtkEl *view_toggle_split(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_toggle_split_button(
	  ui, "Bold", (XtkSplitDesc) {.icon = "Bold", .checked = true, .item_count = 0}
	);
}

static int check_toggle_split(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) store;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, "toggle split type");
	XentRect r = node_rect(ctx, rt->root);
	EXPECT(r.w >= 48.0f, "toggle split has width");
	return 0;
}

/* ---------------------------------------------------------------- BreadcrumbBar */

static char const *const kCrumbs [] = {"Home", "Docs", "File"};

static XtkEl *view_breadcrumb(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_breadcrumb_bar(ui, (XtkBreadcrumbDesc) {.items = kCrumbs, .count = 3});
}

static int check_breadcrumb(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) store;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_BREADCRUMB_BAR, "breadcrumb type");
	XentRect r = node_rect(ctx, rt->root);
	EXPECT(r.w >= 180.0f && r.h >= 28.0f, "breadcrumb has size");
	return 0;
}

/* ---------------------------------------------------------------- SelectorBar */

static XtkSelectorItem const kSel [] = {
  {.text = "All"},
  {.text = "Docs"},
  {.text = "Media"},
};

static XtkEl *view_selector(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_selector_bar(
	  ui, (XtkSelectorBarDesc) {.items = kSel, .item_count = 3, .selected = 1}
	);
}

static int check_selector(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) store;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_SELECTOR_BAR, "selector bar type");
	XentRect r = node_rect(ctx, rt->root);
	EXPECT(r.h >= 32.0f && r.w >= 120.0f, "selector bar has size");
	return 0;
}

/* TeachingTip requires an app-hosted window; covered by the Gallery page instead. */
/* ---------------------------------------------------------------- TreeView */

static XtkTreeNodeDesc const kTreeKids [] = {
  {.text = "Child A"},
  {.text = "Child B"},
};

static XtkTreeNodeDesc const kTreeRoots [] = {
  {.text = "Root", .expanded = true, .children = kTreeKids, .child_count = 2},
};

static XtkEl *view_tree(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_tree_view(
	    ui, (XtkTreeDesc) {.roots = kTreeRoots, .root_count = 1, .row_height = 28.0f}
	  ),
	  400.0f, 240.0f
	);
}

static int check_tree(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_TREE_VIEW, "tree view type");
	FluxTreeViewData *td = ( FluxTreeViewData * ) nd->component_data;
	EXPECT(td && td->flat_count == 3, "expanded tree flattens to three rows");
	EXPECT(td->list != XENT_NODE_INVALID, "tree owns list spine");
	return 0;
}

/* ---------------------------------------------------------------- ItemsView */

static XtkEl *items_cell(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "%d", index), (XtkTextDesc) {0});
}

static XtkEl *view_items(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_items_view(
	    ui, (XtkItemsViewDesc) {
	          .count       = 50,
	          .item_height = 48.0f,
	          .item_width  = 120.0f,
	          .layout      = XTK_ITEMS_LAYOUT_FLOW,
	          .item        = items_cell,
	        }
	  ),
	  500.0f, 300.0f
	);
}

static int check_items(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_ITEMS_VIEW, "items view type");
	FluxListViewData *ld = ( FluxListViewData * ) nd->component_data;
	EXPECT(ld && ld->scroll != XENT_NODE_INVALID && ld->host != XENT_NODE_INVALID, "items spine");
	return 0;
}

/* ---------------------------------------------------------------- PullToRefresh */

static XtkEl *refresh_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "row %d", index), (XtkTextDesc) {0});
}

static XtkEl *view_refresh(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_refresh_container(
	    ui, (XtkRefreshDesc) {0},
	    (XtkEl *[]) {
	      xtk_list_view(
	        ui, (XtkListDesc) {.count = 20, .item_height = 40.0f, .item = refresh_row}
	      ),
	      XTK_END}
	  ),
	  400.0f, 300.0f
	);
}

static int check_refresh(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) rt;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_REFRESH, "refresh container type");
	FluxRefreshData *rd = ( FluxRefreshData * ) nd->component_data;
	EXPECT(rd && rd->scroll_child != XENT_NODE_INVALID, "refresh wraps scroll child");
	FluxNodeData *scroll = flux_node_store_get(store, rd->scroll_child);
	EXPECT(scroll && scroll->component_type == FLUX_CONTROL_SCROLL, "scroll child type");
	( void ) ctx;
	return 0;
}

/* ---------------------------------------------------------------- PersonPicture */

static XtkEl *view_person(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_person_picture(
	  ui, (XtkPersonPictureDesc) {.display_name = "John Smith", .badge_number = 128, .size = 80.0f}
	);
}

static int check_person(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_PERSON_PICTURE, "person picture type");
	FluxPersonPictureData *pd = ( FluxPersonPictureData * ) nd->component_data;
	EXPECT(pd && strcmp(pd->resolved, "JS") == 0, "initials derive from first+last word");
	EXPECT(pd->badge_number == 128, "badge number carried");
	EXPECT(pd->diameter == 80.0f, "requested diameter carried");
	( void ) ctx;
	return 0;
}

/* ---------------------------------------------------------------- PagerControl */

static XtkEl *view_pager(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_pager(ui, (XtkPagerDesc) {.count = 32, .selected = 6, .first_button = XTK_PAGER_BUTTON_AUTO});
}

static int check_pager(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_PAGER, "pager type");
	FluxPagerData *pd = ( FluxPagerData * ) nd->component_data;
	EXPECT(pd && pd->count == 32 && pd->selected == 6, "pager config carried");

	/* Center-ellipsis window for sel0=6: 1 … 6 7 8 … 32 (selected page 7 centered). */
	int16_t cells [FLUX_PAGER_MAX_CELLS];
	int     cc = flux_pager_build_cells(pd->count, pd->selected, cells);
	EXPECT(cc == 7, "center window has seven cells");
	EXPECT(cells [0] == 1 && cells [6] == 32, "first and last pages anchored");
	EXPECT(cells [1] == FLUX_PAGER_ELLIPSIS_PAGE && cells [5] == FLUX_PAGER_ELLIPSIS_PAGE, "both ellipses present");
	EXPECT(cells [3] == pd->selected + 1, "selected page centered");
	return 0;
}

/* ---------------------------------------------------------------- SplitView */

static XtkEl *view_split(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_split_view(
	    ui, (XtkSplitViewDesc) {.pane_open = true, .display_mode = XTK_SPLITVIEW_INLINE},
	    xtk_text(ui, "pane", (XtkTextDesc) {0}), xtk_text(ui, "content", (XtkTextDesc) {0})
	  ),
	  600.0f, 300.0f
	);
}

static int check_split(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	XentNodeId    root = rt->root->node;
	FluxNodeData *nd   = flux_node_store_get(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_SPLIT_VIEW, "split view type");
	FluxSplitViewData *d = ( FluxSplitViewData * ) nd->component_data;
	EXPECT(d && d->pane_open && d->display_mode == FLUX_SPLITVIEW_INLINE, "split config carried");
	EXPECT(d->open_len == FLUX_SPLITVIEW_OPEN_LEN, "default open pane length");

	XentNodeId content = xent_get_first_child(ctx, root);
	XentNodeId pane    = content != XENT_NODE_INVALID ? xent_get_next_sibling(ctx, content) : XENT_NODE_INVALID;
	FluxNodeData *cnd  = flux_node_store_get(store, content);
	FluxNodeData *pnd  = flux_node_store_get(store, pane);
	EXPECT(cnd && cnd->component_type == FLUX_CONTROL_SPLIT_VIEW_CONTENT, "content wrapper first");
	EXPECT(pnd && pnd->component_type == FLUX_CONTROL_SPLIT_VIEW_PANE, "pane wrapper second");

	/* Drive the layout sync the engine would run each frame, then confirm the
	 * pane wrapper picked up the (inline) surface state. */
	flux_split_view_sync(ctx, root, nd);
	FluxSplitPaneData *pd = ( FluxSplitPaneData * ) pnd->component_data;
	EXPECT(pd && !pd->overlay && pd->divider, "inline pane: divider, no overlay surface");
	return 0;
}

/* ---------------------------------------------- PullToRefresh touch gesture */

static void refresh_touch_cb(void *ud, int dir) {
	( void ) dir;
	(*( int * ) ud)++;
}

static int test_refresh_touch(void) {
	XentConfig     cfg   = {0};
	XentContext   *ctx   = xent_create_context(&cfg);
	FluxNodeStore *store = flux_node_store_create(64);
	flux_node_store_bind_context(store, ctx);
	XentNodeId host = xent_create_node(ctx);

	int        fired = 0;
	XentNodeId root  = flux_create_refresh(&(FluxRefreshContainerCreateInfo) {
	   .ctx        = ctx,
	   .store      = store,
	   .parent     = host,
	   .direction  = FLUX_PULL_TOP_TO_BOTTOM,
	   .on_refresh = refresh_touch_cb,
	   .userdata   = &fired});
	EXPECT(root != XENT_NODE_INVALID, "refresh container created");

	FluxNodeData    *nd = flux_node_store_get(store, root);
	FluxRefreshData *d  = ( FluxRefreshData * ) nd->component_data;

	/* Default visualizer 100dip, execution ratio 0.8 -> threshold 80dip. A 20dip
	 * over-pull arms and interacts; 90dip crosses the threshold to Pending. */
	flux_refresh_touch_pull(store, root, 0.0f, -20.0f, 0.0f, 0.0f);
	EXPECT(d->state == FLUX_REFRESH_INTERACTING, "small over-pull interacts");
	flux_refresh_touch_pull(store, root, 0.0f, -90.0f, 0.0f, 0.0f);
	EXPECT(d->state == FLUX_REFRESH_PENDING, "over-pull past threshold pends");
	flux_refresh_touch_release(store, root);
	EXPECT(d->state == FLUX_REFRESH_REFRESHING, "release past threshold refreshes");
	EXPECT(fired == 1, "on_refresh fired once");

	/* Releasing below the threshold retracts without refreshing. */
	flux_refresh_set_refreshing(store, root, false);
	flux_refresh_touch_pull(store, root, 0.0f, -30.0f, 0.0f, 0.0f);
	flux_refresh_touch_release(store, root);
	EXPECT(d->state == FLUX_REFRESH_IDLE, "below-threshold release retracts");
	EXPECT(fired == 1, "no extra refresh below threshold");

	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: pull-to-refresh touch\n");
	return 0;
}

/* ---------------------------------------------------------------- TitleBar */

static XtkEl *view_title_bar(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_title_bar(
	    ui, "Fluxent", (XtkTitleBarDesc) {.subtitle = "Preview", .icon = "Home", .show_back = true, .show_pane_toggle = true}
	  ),
	  600.0f, 48.0f
	);
}

static int check_title_bar(XentContext *ctx, FluxNodeStore *store, XtkRuntime *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_node_store_get(store, rt->root->node);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_TITLE_BAR, "title bar type");
	FluxTitleBarData *d = ( FluxTitleBarData * ) nd->component_data;
	EXPECT(d && d->title && strcmp(d->title, "Fluxent") == 0, "title carried");
	EXPECT(d->show_back && d->show_pane_toggle, "back + pane-toggle enabled");
	EXPECT(d->icon_glyph [0] != '\0', "icon resolved to a glyph");
	EXPECT(d->title_w > 0.0f, "title measured for subtitle placement");

	/* Back button sits at the far left; its hit box maps to the back region. */
	FluxRect           b = {0.0f, 0.0f, 600.0f, FLUX_TB_HEIGHT};
	FluxTitleBarLayout l = flux_titlebar_layout(&b, true, true, true, false);
	EXPECT(l.has_back && l.back.x < l.toggle.x, "back precedes pane-toggle");
	EXPECT(l.title_x > l.toggle.x, "title follows the buttons");
	return 0;
}

int main(void) {
	int m = 0;
	if (run_view("radio buttons", &m, view_radio_buttons, check_radio_buttons)) return 1;
	if (run_view("rating", &m, view_rating, check_rating)) return 1;
	if (run_view("toggle split button", &m, view_toggle_split, check_toggle_split)) return 1;
	if (run_view("breadcrumb bar", &m, view_breadcrumb, check_breadcrumb)) return 1;
	if (run_view("selector bar", &m, view_selector, check_selector)) return 1;
	if (run_view("tree view", &m, view_tree, check_tree)) return 1;
	if (run_view("items view", &m, view_items, check_items)) return 1;
	if (run_view("pull to refresh", &m, view_refresh, check_refresh)) return 1;
	if (run_view("person picture", &m, view_person, check_person)) return 1;
	if (run_view("pager control", &m, view_pager, check_pager)) return 1;
	if (run_view("split view", &m, view_split, check_split)) return 1;
	if (run_view("title bar", &m, view_title_bar, check_title_bar)) return 1;
	if (test_refresh_touch()) return 1;
	printf("PASS: all new-control probes\n");
	return 0;
}
