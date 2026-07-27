/**
 * @file test_fx_new_controls.c
 * @brief Headless mount/layout probes for the nine new WinUI 3 controls:
 * RadioButtons, RatingControl, ToggleSplitButton, BreadcrumbBar, SelectorBar,
 * TeachingTip, TreeView, ItemsView, and PullToRefresh.
 */
#include "flux_internal.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_host.h>

#include "controls/data/flux_list_view_data.h"
#include "controls/data/flux_items_view_data.h"
#include "controls/data/flux_info_bar_data.h"
#include "controls/data/flux_pager_data.h"
#include "controls/data/flux_person_picture_data.h"
#include "controls/data/flux_radio_buttons_data.h"
#include "controls/data/flux_slider_data.h"
#include "controls/data/flux_split_view_data.h"
#include "controls/data/flux_selector_bar_data.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/data/flux_title_bar_data.h"
#include "controls/data/flux_text_data.h"
#include "controls/behavior/flux_expander_internal.h"
#include "controls/behavior/flux_breadcrumb_internal.h"
#include "controls/behavior/flux_flip_internal.h"
#include "controls/behavior/flux_refresh_internal.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/factory/flux_factory.h"

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

static int   g_breadcrumb_click = -1;
static float g_slider_value     = -1.0f;

static void  noop_update(void *model, XtkMsg msg) {
	( void ) model;
	if (msg.tag == 91) g_breadcrumb_click = msg.i;
	if (msg.tag == 92) g_slider_value = msg.f;
}

static XentRect node_rect(XentCtx *ctx, XtkNode const *n) {
	XentRect r = {0};
	if (n) xent_layout_rect(ctx, xtk_node_id(n), &r);
	return r;
}

static int run_view(
  char const *name, void *model, XtkEl *(*build)( XtkUi *, void * ), int (*check)(XentCtx *, FluxStore *, XtkRt *)
) {
	XentCfg    config = {0};
	XentCtx   *ctx    = xent_ctx_create(&config);
	FluxStore *store  = flux_store_create(128);
	flux_store_bind(store, ctx);

	XentNodeId host = xent_node_create(ctx);
	xent_setproto(ctx, host, XENT_PROTOCOL_FLEX);

	FluxBackendCtx bctx    = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkHost       *xtkhost = flux_host_adopt(ctx, store, host);
	EXPECT(xtkhost, "host adopt");
	bctx.host    = xtkhost;
	XtkRtCfg cfg = flux_xtk_runtime_config(&bctx);
	XtkRt   *rt  = xtk_rt_create(xtkhost, &cfg, host, model, noop_update, build);
	EXPECT(rt, "runtime");
	bctx.runtime = rt;

	xtk_rt_frame(rt);
	xent_layout(ctx, host, 900.0f, 640.0f);

	int rc = check(ctx, store, rt);
	xtk_rt_destroy(rt);
	flux_host_destroy(xtkhost);
	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
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
	return xtk_radiogroup(
	  ui, (XtkRadioButtonsDesc) {
			.header = "Group",
			.items  = kRb,
			.nitem  = 3,
			.sel    = 1,
		  }
	);
}

static int check_radio_buttons(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_RADIO_BUTTONS, "radio buttons data");
	FluxRadioButtonsData *g = ( FluxRadioButtonsData * ) nd->component_data;
	EXPECT(g && g->count == 3, "three radio items");
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(r.h >= 96.0f, "radio group has vertical extent");
	return 0;
}

/* ---------------------------------------------------------------- Rating */

static XtkEl *view_rating(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_rating(ui, (XtkRatingDesc) {.value = 3.0, .max_rating = 5, .caption = "Stars"});
}

static int check_rating(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) store;
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(r.w >= 120.0f && r.h >= 28.0f, "rating strip has size");
	return 0;
}

/* ---------------------------------------------------------------- ToggleSplitButton */

static XtkEl *view_toggle_split(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_togglesplit(ui, "Bold", (XtkSplitDesc) {.icon = "Bold", .checked = true, .nitem = 0});
}

static int check_toggle_split(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) store;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, "toggle split type");
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(r.w >= 100.0f, "toggle split reserves primary content and secondary zone");
	return 0;
}

/* ---------------------------------------------------------------- Slider */

static XtkEl *view_slider(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_slider(
		ui,
		(XtkSliderDesc) {
		  .min            = 500.0f,
		  .max            = 1000.0f,
		  .value          = 650.0f,
		  .step           = 10.0f,
		  .tick           = 20.0f,
		  .tick_placement = XTK_TICK_OUTSIDE,
		  .snap_to_ticks  = true,
		  .small_change   = 10.0f,
		  .large_change   = 100.0f,
		  .change         = xtk_msg(92)}
	  ),
	  300.0f, NAN
	);
}

static int check_slider(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_SLIDER, "slider type");
	FluxSliderData *d = ( FluxSliderData * ) nd->component_data;
	EXPECT(d && d->min_value == 500.0f && d->max_value == 1000.0f, "slider range projected");
	EXPECT(d->current_value == 650.0f && d->step_frequency == 10.0f, "slider value and step projected");
	EXPECT(d->tick_frequency == 20.0f && d->tick_placement == XTK_TICK_OUTSIDE, "slider ticks projected");
	EXPECT(d->snaps_to == FLUX_SLIDER_SNAPS_TO_TICKS, "slider snap mode projected");
	EXPECT(d->small_change == 10.0f && d->large_change == 100.0f, "slider keyboard changes projected");
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(nd->behavior.on_pointer_move && r.w > 20.0f, "slider pointer behavior");
	g_slider_value = -1.0f;
	nd->behavior.on_pointer_move(nd->behavior.on_pointer_move_ctx, r.w * 0.5f, r.h * 0.5f);
	xtk_rt_frame(rt);
	EXPECT(g_slider_value == 740.0f, "slider pointer value routes through descriptor binding");
	return 0;
}

/* ---------------------------------------------------------------- BreadcrumbBar */

static char const *const kCrumbs [] = {"Home", "Docs", "File"};

static XtkEl            *view_breadcrumb(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_breadcrumb(ui, (XtkBreadcrumbDesc) {.items = kCrumbs, .nitem = 3, .click = xtk_msg(91)});
}

static int check_breadcrumb(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_BREADCRUMB_BAR, "breadcrumb type");
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(r.w >= 180.0f && r.h >= 26.0f, "breadcrumb has size");
	FluxBreadcrumbBarData *d    = ( FluxBreadcrumbBarData * ) nd->component_data;
	FluxNodeData          *last = d ? flux_store_find(store, d->nodes [d->count]) : NULL;
	EXPECT(last && last->behavior.on_click, "last breadcrumb is interactive");
	last->hover_local_x = 1.0f;
	g_breadcrumb_click  = -1;
	last->behavior.on_click(last->behavior.on_click_ctx);
	xtk_rt_frame(rt);
	EXPECT(g_breadcrumb_click == 2, "last breadcrumb posts ItemClicked");
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
	return xtk_selector(ui, (XtkSelectorBarDesc) {.items = kSel, .nitem = 3, .sel = 1});
}

static int check_selector(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	XentNodeId    root = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *nd   = flux_store_find(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_SELECTOR_BAR, "selector bar type");
	XentRect r = node_rect(ctx, xtk_rt_root(rt));
	EXPECT(r.h >= 32.0f && r.w >= 120.0f, "selector bar has size");
	FluxSelectorBarData *bar         = ( FluxSelectorBarData * ) nd->component_data;
	FluxNodeData        *viewport_nd = bar ? flux_store_find(store, bar->viewport) : NULL;
	FluxScrollData      *scroll      = viewport_nd ? ( FluxScrollData * ) viewport_nd->component_data : NULL;
	EXPECT(
	  viewport_nd && viewport_nd->component_type == FLUX_CONTROL_SCROLL && scroll,
	  "selector bar owns horizontal scroll viewport"
	);
	EXPECT(scroll->h_vis == FLUX_SCROLL_NEVER && scroll->v_vis == FLUX_SCROLL_NEVER, "selector scrollbars stay hidden");
	XentNodeId host = xent_node_parent(ctx, root);
	EXPECT(xent_layout(ctx, host, 140.0f, 100.0f), "narrow selector layout");
	XentRect viewport = {0};
	XentRect first    = {0};
	XentRect last     = {0};
	EXPECT(xent_layout_rect(ctx, bar->viewport, &viewport), "selector viewport rect");
	EXPECT(xent_layout_rect(ctx, bar->items [0], &first), "selector first item rect");
	EXPECT(xent_layout_rect(ctx, bar->items [bar->count - 1], &last), "selector last item rect");
	EXPECT(first.w > 30.0f && last.w > 40.0f, "narrow selector keeps item widths");
	EXPECT(last.x + last.w > viewport.x + viewport.w, "narrow selector overflows into scroll extent");
	FluxNodeData *last_nd = flux_store_find(store, bar->items [bar->count - 1]);
	EXPECT(last_nd && last_nd->behavior.on_key, "selector item has keyboard behavior");
	last_nd->behavior.on_key(last_nd->behavior.on_key_ctx, FLUX_KEY_END, true);
	EXPECT(scroll->scroll_x > 0.0f, "selector keyboard navigation reveals the focused item");
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
	  xtk_tree(ui, (XtkTreeDesc) {.roots = kTreeRoots, .nroot = 1, .sel = -1, .rowh = 28.0f}), 400.0f, 240.0f
	);
}

static int check_tree(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
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
	  xtk_itemsview(
		ui,
		(XtkItemsViewDesc) {
		  .nitem                = 50,
		  .itemh                = 48.0f,
		  .itemw                = 120.0f,
		  .gap                  = 8.0f,
		  .layout               = XTK_ITEMS_LAYOUT_FLOW,
		  .item                 = items_cell,
		  .selection_mode       = XTK_LIST_SELECT_NONE,
		  .item_invoked_enabled = true,
		}
	  ),
	  500.0f, 300.0f
	);
}

static int check_items(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_ITEMS_VIEW, "items view type");
	FluxItemsViewData *items = ( FluxItemsViewData * ) nd->component_data;
	FluxListViewData  *ld    = items ? &items->list : NULL;
	EXPECT(ld && ld->scroll != XENT_NODE_INVALID && ld->host != XENT_NODE_INVALID, "items spine");
	EXPECT(ld->sel_mode == XTK_LIST_SELECT_NONE, "items selection mode carried");
	EXPECT(ld->item_invoked_enabled, "items invoked gate carried");
	EXPECT(ld->item_height == 56.0f && ld->item_width == 128.0f, "items spacing folded into pitch");
	EXPECT(xent_node_nchild(ctx, ld->host) > 0, "items view realizes visible cells");
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
	  xtk_refresh(
		ui,
		(XtkRefreshDesc) {
		  .direction       = XTK_PULL_LEFT_TO_RIGHT,
		  .visualizer_size = 144.0f,
		  .threshold_ratio = 0.65f,
		  .is_refreshing   = true,
		},
		(XtkEl *[]) {xtk_list(ui, (XtkListDesc) {.nitem = 20, .itemh = 40.0f, .item = refresh_row}), XTK_END}
	  ),
	  400.0f, 300.0f
	);
}

static int check_refresh(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) rt;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_REFRESH, "refresh container type");
	FluxRefreshData *rd = ( FluxRefreshData * ) nd->component_data;
	EXPECT(rd && rd->scroll_child != XENT_NODE_INVALID, "refresh wraps scroll child");
	EXPECT(rd->direction == FLUX_PULL_LEFT_TO_RIGHT, "refresh direction carried");
	EXPECT(rd->visualizer_size == 144.0f && rd->threshold_ratio == 0.65f, "refresh thresholds carried");
	EXPECT(rd->state == FLUX_REFRESH_REFRESHING, "refresh controlled state carried");
	FluxNodeData *scroll = flux_store_find(store, rd->scroll_child);
	EXPECT(scroll && scroll->component_type == FLUX_CONTROL_SCROLL, "scroll child type");
	XentNodeId    content = xent_node_first(ctx, rd->scroll_child);
	FluxNodeData *list    = flux_store_find(store, content);
	EXPECT(list && list->component_type == FLUX_CONTROL_LIST, "refresh content mounts under scroll");
	FluxListViewData *ld = list ? ( FluxListViewData * ) list->component_data : NULL;
	EXPECT(ld && xent_node_nchild(ctx, ld->host) > 0, "refresh list realizes visible rows");
	return 0;
}

/* ---------------------------------------------------------------- InfoBar */

static XtkEl *view_info_bar(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_infobar(
	  ui, "Network", "Connection restored.", (XtkInfoBarDesc) {.severity = XTK_INFOBAR_SUCCESS, .closable = true}
	);
}

static int check_info_bar(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	XentNodeId    root = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *nd   = flux_store_find(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_INFO_BAR, "info bar type");
	FluxInfoBarData *d = ( FluxInfoBarData * ) nd->component_data;
	EXPECT(d && strcmp(d->title, "Network") == 0, "info bar title carried");
	EXPECT(strcmp(d->message, "Connection restored.") == 0, "info bar message carried");
	EXPECT(d->severity == XTK_INFOBAR_SUCCESS && d->is_closable, "info bar state carried");
	EXPECT(xent_get_focusable(ctx, root), "closable info bar is focusable");
	XentRect rect = {0};
	EXPECT(xent_layout_rect(ctx, root, &rect) && rect.w > 43.0f, "info bar has close target");
	EXPECT(nd->behavior.on_pointer_down && nd->behavior.on_click, "info bar close behavior");
	nd->behavior.on_pointer_down(nd->behavior.on_pointer_down_ctx, rect.w - 1.0f, 1.0f, 1);
	nd->behavior.on_click(nd->behavior.on_click_ctx);
	EXPECT(!d->is_open, "info bar close target closes");
	EXPECT(xent_layout(ctx, root, 900.0f, 640.0f), "closed info bar relayout");
	EXPECT(xent_layout_rect(ctx, root, &rect) && rect.w == 0.0f && rect.h == 0.0f, "closed info bar collapses");
	return 0;
}

/* ---------------------------------------------------------------- Expander */

static XtkEl *view_expander(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_expander(
	  ui, "Details", (XtkExpanderDesc) {.expanded = true, .w = 420.0f, .contenth = 96.0f},
	  (XtkEl *[]) {xtk_text(ui, "Body", (XtkTextDesc) {0}), XTK_END}
	);
}

static int check_expander(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	XentNodeId    root = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *nd   = flux_store_find(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_EXPANDER, "expander type");
	FluxExpanderData *d = ( FluxExpanderData * ) nd->component_data;
	EXPECT(d && d->expanded, "expander state carried");
	EXPECT(d->width == 420.0f && d->content_height == 96.0f, "expander dimensions carried");
	EXPECT(xent_node_parent(ctx, d->content) == root, "expanded content is attached");
	EXPECT(xent_node_nchild(ctx, d->content) == 1, "expander body mounts under content host");
	XentRect body  = {0};
	XentRect child = {0};
	EXPECT(xent_layout_rect(ctx, d->content, &body), "expander body rect");
	EXPECT(xent_layout_rect(ctx, xent_node_first(ctx, d->content), &child), "expander content rect");
	EXPECT(body.w == 420.0f && child.w >= 32.0f, "expander content keeps usable width");
	FluxNodeData *text_nd = flux_store_find(store, d->header_text);
	FluxTextData *text    = text_nd ? ( FluxTextData * ) text_nd->component_data : NULL;
	EXPECT(text && strcmp(text->content, "Details") == 0, "expander header carried");
	return 0;
}

/* ---------------------------------------------------------------- FlipView */

static XtkEl *view_flip(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_flip(
		ui, (XtkFlipDesc) {.sel = 2, .vertical = true},
		(XtkEl *[]) {
		  xtk_text(ui, "one", (XtkTextDesc) {0}), xtk_text(ui, "two", (XtkTextDesc) {0}),
		  xtk_text(ui, "three", (XtkTextDesc) {0}), XTK_END}
	  ),
	  420.0f, 240.0f
	);
}

static int check_flip(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	XentNodeId    root = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *nd   = flux_store_find(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_FLIP_VIEW, "flip view type");
	FluxFlipViewData *d = ( FluxFlipViewData * ) nd->component_data;
	EXPECT(d && d->selected == 2 && d->vertical, "flip state carried");
	EXPECT(xent_node_nchild(ctx, d->host) == 3, "flip pages mount under content host");
	return 0;
}

/* ---------------------------------------------------------------- PipsPager */

static XtkEl *view_pips(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_pips(
	  ui, (XtkPipsDesc) {
			.nitem       = 11,
			.sel         = 7,
			.max_visible = 5,
			.vertical    = true,
			.nav         = XTK_PIPS_NAV_VISIBLE,
		  }
	);
}

static int check_pips(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
	EXPECT(nd && nd->component_type == FLUX_CONTROL_PIPS_PAGER, "pips pager type");
	FluxPipsPagerData *d = ( FluxPipsPagerData * ) nd->component_data;
	EXPECT(d && d->count == 11 && d->selected == 7, "pips count and selection carried");
	EXPECT(d->max_visible == 5 && d->vertical, "pips layout state carried");
	EXPECT(d->nav_vis == XTK_PIPS_NAV_VISIBLE, "pips navigation visibility carried");
	return 0;
}

/* ---------------------------------------------------------------- PersonPicture */

static XtkEl *view_person(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_avatar(ui, (XtkPersonPictureDesc) {.display_name = "John Smith", .badge_number = 128, .size = 80.0f});
}

static int check_person(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
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
	return xtk_pager(ui, (XtkPagerDesc) {.nitem = 32, .sel = 6, .first_button = XTK_PAGER_BUTTON_AUTO});
}

static int check_pager(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
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
	  xtk_splitview(
		ui, (XtkSplitViewDesc) {.pane_open = true, .display_mode = XTK_SPLITVIEW_INLINE},
		xtk_text(ui, "pane", (XtkTextDesc) {0}), xtk_text(ui, "content", (XtkTextDesc) {0})
	  ),
	  600.0f, 300.0f
	);
}

static int check_split(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	XentNodeId    root = xtk_node_id(xtk_rt_root(rt));
	FluxNodeData *nd   = flux_store_find(store, root);
	EXPECT(nd && nd->component_type == FLUX_CONTROL_SPLIT_VIEW, "split view type");
	FluxSplitViewData *d = ( FluxSplitViewData * ) nd->component_data;
	EXPECT(d && d->pane_open && d->display_mode == FLUX_SPLITVIEW_INLINE, "split config carried");
	EXPECT(d->open_len == FLUX_SPLITVIEW_OPEN_LEN, "default open pane length");

	XentNodeId    content = xent_node_first(ctx, root);
	XentNodeId    pane    = content != XENT_NODE_INVALID ? xent_node_next(ctx, content) : XENT_NODE_INVALID;
	FluxNodeData *cnd     = flux_store_find(store, content);
	FluxNodeData *pnd     = flux_store_find(store, pane);
	EXPECT(cnd && cnd->component_type == FLUX_CONTROL_SPLIT_VIEW_CONTENT, "content wrapper first");
	EXPECT(pnd && pnd->component_type == FLUX_CONTROL_SPLIT_VIEW_PANE, "pane wrapper second");

	/* Drive the layout sync the engine would run each frame, then confirm the
	 * pane wrapper picked up the (inline) surface state. */
	flux_splitview_sync(ctx, root, nd);
	FluxSplitPaneData *pd = ( FluxSplitPaneData * ) pnd->component_data;
	EXPECT(pd && !pd->overlay && pd->divider, "inline pane: divider, no overlay surface");
	return 0;
}

/* ---------------------------------------------- PullToRefresh touch gesture */

static void refresh_touch_cb(void *ud, int dir) {
	( void ) dir;
	(*( int * ) ud)++;
}

static void count_click(void *ctx) { (*( int * ) ctx)++; }

typedef struct CancelProbe {
	void  (*next)(void *);
	void *next_ctx;
	int   count;
} CancelProbe;

static void count_cancel(void *ctx) {
	CancelProbe *probe = ( CancelProbe * ) ctx;
	probe->count++;
	if (probe->next) probe->next(probe->next_ctx);
}

static int test_refresh_touch(void) {
	XentCfg    cfg   = {0};
	XentCtx   *ctx   = xent_ctx_create(&cfg);
	FluxStore *store = flux_store_create(64);
	flux_store_bind(store, ctx);
	XentNodeId host  = xent_node_create(ctx);

	int        fired = 0;
	XentNodeId root  = flux_refresh_init(&(FluxRefreshContainerDesc) {
	  .ctx        = ctx,
	  .store      = store,
	  .parent     = host,
	  .direction  = FLUX_PULL_TOP_TO_BOTTOM,
	  .on_refresh = refresh_touch_cb,
	  .userdata   = &fired});
	EXPECT(root != XENT_NODE_INVALID, "refresh container created");

	FluxNodeData    *nd = flux_store_find(store, root);
	FluxRefreshData *d  = ( FluxRefreshData * ) nd->component_data;

	/* Default visualizer 100dip, execution ratio 0.8 -> threshold 80dip. A 20dip
	 * over-pull arms and interacts; 90dip crosses the threshold to Pending. */
	flux_refresh_touch_pull(store, root, 0.0f, -20.0f, 0.0f, 0.0f);
	EXPECT(d->state == FLUX_REFRESH_INTERACTING, "small over-pull interacts");
	FluxNodeData *scroll_nd = flux_store_find(store, d->scroll_child);
	EXPECT(scroll_nd && scroll_nd->render_translate_y == 20.0f, "pull translates wrapped content");
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

	/* Real input path: a nested scroller must still discover the surrounding
	 * RefreshContainer. Dragging down from its top edge crosses the same
	 * threshold and suppresses the descendant click. */
	XentNodeId outer = flux_refresh_content_node(store, root);
	XentNodeId inner = flux_scroll_init(&(FluxContainerDesc) {ctx, store, outer});
	XentNodeId body  = flux_factory_create_node(ctx, store, inner, FLUX_CONTROL_CONTAINER);
	xent_setsize(ctx, host, (XentSize) {300.0f, 300.0f});
	xent_setsize(ctx, root, (XentSize) {300.0f, 300.0f});
	xent_setsize(ctx, inner, (XentSize) {300.0f, 300.0f});
	xent_setsize(ctx, body, (XentSize) {300.0f, 800.0f});
	EXPECT(xent_layout(ctx, host, 300.0f, 300.0f), "refresh input layout");
	FluxNodeData   *inner_nd = flux_store_find(store, inner);
	FluxScrollData *inner_sd = inner_nd ? ( FluxScrollData * ) inner_nd->component_data : NULL;
	EXPECT(inner_sd, "nested refresh scroller");
	inner_sd->content_w = 300.0f;
	inner_sd->content_h = 800.0f;

	FluxInput *input    = flux_input_create(ctx, store);
	EXPECT(input, "refresh input router");
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind           = FLUX_POINTER_DOWN,
		.device         = FLUX_POINTER_MOUSE,
		.x              = 50.0f,
		.y              = 50.0f,
		.buttons        = FLUX_POINTER_BUTTON_LEFT,
		.changed_button = FLUX_POINTER_BUTTON_LEFT}
	);
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind    = FLUX_POINTER_MOVE,
		.device  = FLUX_POINTER_MOUSE,
		.x       = 50.0f,
		.y       = 140.0f,
		.buttons = FLUX_POINTER_BUTTON_LEFT}
	);
	EXPECT(d->state == FLUX_REFRESH_PENDING, "nested mouse pull reaches refresh");
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind           = FLUX_POINTER_UP,
		.device         = FLUX_POINTER_MOUSE,
		.x              = 50.0f,
		.y              = 140.0f,
		.changed_button = FLUX_POINTER_BUTTON_LEFT}
	);
	EXPECT(d->state == FLUX_REFRESH_REFRESHING, "mouse release requests refresh");
	EXPECT(fired == 2, "real input path fires refresh once");
	flux_input_destroy(input);

	int        repeats = 0;
	XentNodeId repeat  = flux_repeat_init(&(FluxButtonDesc) {
	  .ctx = ctx, .store = store, .parent = body, .label = "Hold", .on_click = count_click, .userdata = &repeats});
	xent_setsize(ctx, repeat, (XentSize) {120.0f, 40.0f});
	EXPECT(xent_layout(ctx, host, 300.0f, 300.0f), "repeat touch layout");
	FluxNodeData *repeat_nd = flux_store_find(store, repeat);
	EXPECT(repeat_nd && repeat_nd->behavior.on_cancel, "repeat has cancel behavior");
	CancelProbe probe = {.next = repeat_nd->behavior.on_cancel, .next_ctx = repeat_nd->behavior.on_cancel_ctx};
	repeat_nd->behavior.on_cancel     = count_cancel;
	repeat_nd->behavior.on_cancel_ctx = &probe;

	input                             = flux_input_create(ctx, store);
	EXPECT(input, "repeat touch input router");
	float repeat_y = 20.0f + d->content_offset;
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind           = FLUX_POINTER_DOWN,
		.device         = FLUX_POINTER_TOUCH,
		.pointer_id     = 1,
		.x              = 50.0f,
		.y              = repeat_y,
		.buttons        = FLUX_POINTER_BUTTON_LEFT,
		.changed_button = FLUX_POINTER_BUTTON_LEFT}
	);
	EXPECT(flux_input_pressed(input) == repeat, "touch presses repeat");
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind       = FLUX_POINTER_HOLD,
		.device     = FLUX_POINTER_TOUCH,
		.pointer_id = 1,
		.x          = 50.0f,
		.y          = repeat_y,
		.buttons    = FLUX_POINTER_BUTTON_LEFT}
	);
	EXPECT(flux_input_pressed(input) == repeat, "touch hold keeps repeat pressed");
	EXPECT(probe.count == 0, "touch hold does not cancel repeat timer");
	flux_input_dispatch(
	  input, root,
	  &(FluxPointerEvent) {
		.kind       = FLUX_POINTER_MOVE,
		.device     = FLUX_POINTER_TOUCH,
		.pointer_id = 1,
		.x          = 50.0f,
		.y          = repeat_y + 30.0f,
		.buttons    = FLUX_POINTER_BUTTON_LEFT}
	);
	EXPECT(flux_input_pressed(input) == XENT_NODE_INVALID, "touch pan releases repeat capture");
	EXPECT(probe.count == 1, "touch pan cancels repeat timer");
	flux_input_destroy(input);

	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	printf("PASS: pull-to-refresh pointer input\n");
	return 0;
}

/* ---------------------------------------------------------------- TitleBar */

static XtkEl *view_title_bar(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_sized(
	  xtk_titlebar(
		ui, "Fluxent",
		(XtkTitleBarDesc) {.subtitle = "Preview", .icon = "Home", .show_back = true, .show_pane_toggle = true}
	  ),
	  600.0f, 48.0f
	);
}

static int check_title_bar(XentCtx *ctx, FluxStore *store, XtkRt *rt) {
	( void ) ctx;
	FluxNodeData *nd = flux_store_find(store, xtk_node_id(xtk_rt_root(rt)));
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
	if (run_view("slider", &m, view_slider, check_slider)) return 1;
	if (run_view("breadcrumb bar", &m, view_breadcrumb, check_breadcrumb)) return 1;
	if (run_view("selector bar", &m, view_selector, check_selector)) return 1;
	if (run_view("tree view", &m, view_tree, check_tree)) return 1;
	if (run_view("items view", &m, view_items, check_items)) return 1;
	if (run_view("pull to refresh", &m, view_refresh, check_refresh)) return 1;
	if (run_view("info bar", &m, view_info_bar, check_info_bar)) return 1;
	if (run_view("expander", &m, view_expander, check_expander)) return 1;
	if (run_view("flip view", &m, view_flip, check_flip)) return 1;
	if (run_view("pips pager", &m, view_pips, check_pips)) return 1;
	if (run_view("person picture", &m, view_person, check_person)) return 1;
	if (run_view("pager control", &m, view_pager, check_pager)) return 1;
	if (run_view("split view", &m, view_split, check_split)) return 1;
	if (run_view("title bar", &m, view_title_bar, check_title_bar)) return 1;
	if (test_refresh_touch()) return 1;
	printf("PASS: all new-control probes\n");
	return 0;
}
