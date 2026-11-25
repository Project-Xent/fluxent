#include "app.h"

enum
{
	DEMO_HEADER_TEXT,
	DEMO_HEADER_ROW,
};

static bool        nav_demo_top(Model const *m) { return kNavDemoPaneModes [m->nav_demo_mode] == XTK_NAV_PANE_TOP; }

static char const *nav_demo_header_text(Model const *m) { return nav_demo_top(m) ? "Menu" : ""; }

static char const *nav_demo_selection(GalleryApp *g) {
	return gallery_fmt(g, "Selected: %s", gallery_nav_demo_title(g->model.nav_demo_sel));
}

static void nav_demo_sync(GalleryApp *g) {
	Model const   *m      = &g->model;
	XtkCtrl        view   = g->live_control [MSG_NAV_DEMO];
	XtkChangeStamp change = {0};
	xtk_nav_setdisplay(&g->scene, view, kNavDemoPaneModes [m->nav_demo_mode], change);
	xtk_nav_setsettings_visible(&g->scene, view, !m->nav_demo_hide, change);
	xtk_nav_setalways_show_header(&g->scene, view, !m->nav_demo_hide_header, change);
	xtk_nav_setback_enabled(&g->scene, view, m->nav_demo_sel != GALLERY_NAV_DEMO_HOME, change);
	xtk_nav_setsel(&g->scene, view, m->nav_demo_sel, change);
	xtk_container_seth(&g->scene, g->live_extra [DEMO_HEADER_ROW], nav_demo_top(m) ? 48.0f : 0.0f, change);
	gallery_settext(g, g->live_extra [DEMO_HEADER_TEXT], nav_demo_header_text(m));
	gallery_settext(g, g->live_output [MSG_NAV_DEMO], nav_demo_selection(g));
}

static XtkNodeRef nav_demo_slot(XtkTxn *txn, XtkNodeRef view, XtkNavSlot slot) {
	return xtk_ref_local(
	  xtk_navslot_create(txn, view, &(XtkNavSlotProps) {.set = XTK_NAVSLOT_SLOT_MASK, .slot = slot})
	);
}

static void nav_demo_pane_header(PageBuild *b, XtkNodeRef view) {
	XtkNodeRef parent = nav_demo_slot(&b->txn, view, XTK_NAV_SLOT_HEADER);
	XtkLocalId row    = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_H_MASK | XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_FITW_MASK,
		.h     = nav_demo_top(&b->app->model) ? 48.0f : 0.0f,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_CENTER,
		.fitw  = true}
	);
	XtkLocalId text
	  = ui_text(&b->txn, xtk_ref_local(row), nav_demo_header_text(&b->app->model), 14.0f, FLUX_FONT_REGULAR);
	page_keepx(b, DEMO_HEADER_TEXT, text);
	page_keepx(b, DEMO_HEADER_ROW, row);
}

static XtkLocalId
nav_demo_text(XtkTxn *txn, XtkNodeRef parent, char const *text, float size, uint32_t weight, XentInsets margin) {
	return xtk_text_create(
	  txn, parent,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK
	                | XTK_TEXT_FONTSIZE_MASK
	                | XTK_TEXT_FONTWEIGHT_MASK
	                | XTK_TEXT_ML_MASK
	                | XTK_TEXT_MT_MASK
	                | XTK_TEXT_MR_MASK
	                | XTK_TEXT_MB_MASK,
		.text       = text,
		.fontsize   = size,
		.fontweight = weight,
		.ml         = margin.left,
		.mt         = margin.top,
		.mr         = margin.right,
		.mb         = margin.bottom}
	);
}

static void nav_demo_slots(PageBuild *b, XtkNodeRef view) {
	nav_demo_pane_header(b, view);
	XtkNodeRef search  = nav_demo_slot(&b->txn, view, XTK_NAV_SLOT_SEARCH);
	XtkLocalId suggest = xtk_suggest_create(
	  &b->txn, search,
	  &(XtkSuggestProps) {
		.set = XTK_SUGGEST_PLACEHOLDER_MASK | XTK_SUGGEST_ICON_MASK, .placeholder = "Find a page", .icon = "Search"}
	);
	xtk_txn_setitems(&b->txn, xtk_ref_local(suggest), kNavDemoSuggestions, 2);
	XtkNodeRef header = nav_demo_slot(&b->txn, view, XTK_NAV_SLOT_PAGE_HEADER);
	nav_demo_text(&b->txn, header, "Page header", 28, FLUX_FONT_SEMI_BOLD, (XentInsets) {16, 8, 16, 0});
	XtkNodeRef overlay = nav_demo_slot(&b->txn, view, XTK_NAV_SLOT_CONTENT_OVERLAY);
	nav_demo_text(&b->txn, overlay, "Navigation preview", 12, FLUX_FONT_REGULAR, (XentInsets) {8, 6, 8, 6});
}

static bool nav_demo_preview(PageBuild *b, XtkNodeRef parent) {
	Model const *m   = &b->app->model;
	XtkLocalId   nav = xtk_nav_create(
	  &b->txn, parent,
	  &(XtkNavProps) {
		.set                = XTK_NAV_H_MASK
	                        | XTK_NAV_SEL_MASK
	                        | XTK_NAV_DISPLAY_MASK
	                        | XTK_NAV_PANE_TITLE_MASK
	                        | XTK_NAV_SETTINGS_VISIBLE_MASK
	                        | XTK_NAV_ALWAYS_SHOW_HEADER_MASK
	                        | XTK_NAV_BACK_ENABLED_MASK,
		.h                  = 440.0f,
		.sel                = m->nav_demo_sel,
		.display            = kNavDemoPaneModes [m->nav_demo_mode],
		.pane_title         = "Navigation",
		.settings_visible   = !m->nav_demo_hide,
		.always_show_header = !m->nav_demo_hide_header,
		.back_enabled       = m->nav_demo_sel != GALLERY_NAV_DEMO_HOME}
	);
	XtkNodeRef view = xtk_ref_local(nav);
	if (!xtk_txn_setnav(&b->txn, view, &(XtkNavDesc) {.items = kNavDemoItems, .nitem = GALLERY_NAV_DEMO_ITEMS}))
		return false;
	xtk_nav_onselect(&b->txn, view, page_post, page_bind(b, MSG_NAV_DEMO));
	xtk_nav_onbackrequested(&b->txn, view, page_post, page_bind(b, MSG_NAV_DEMO));
	nav_demo_slots(b, view);
	XtkLocalId body = xtk_container_create(
	  &b->txn, view,
	  &(XtkContainerProps) {
		.set  = XTK_CONTAINER_DIR_MASK
	          | XTK_CONTAINER_FITH_MASK
	          | XTK_CONTAINER_PL_MASK
	          | XTK_CONTAINER_PT_MASK
	          | XTK_CONTAINER_PR_MASK
	          | XTK_CONTAINER_PB_MASK,
		.dir  = XTK_STACK_COLUMN,
		.fith = true,
		.pl   = 16,
		.pt   = 12,
		.pr   = 16,
		.pb   = 12}
	);
	page_keepout(
	  b, MSG_NAV_DEMO, ui_text(&b->txn, xtk_ref_local(body), nav_demo_selection(b->app), 14, FLUX_FONT_REGULAR)
	);
	page_keep(b, MSG_NAV_DEMO, nav);
	return true;
}

static void nav_demo_options(PageBuild *b, XtkNodeRef parent) {
	Model const *m     = &b->app->model;
	XtkNodeRef   row   = xtk_ref_local(ui_row(&b->txn, parent, 12.0f, true));
	XtkLocalId   combo = xtk_combo_create(
	  &b->txn, row,
	  &(XtkComboProps) {
		.set         = XTK_COMBO_PLACEHOLDER_MASK | XTK_COMBO_SEL_MASK,
		.placeholder = "Pane display mode",
		.sel         = m->nav_demo_mode}
	);
	xtk_txn_setitems(&b->txn, xtk_ref_local(combo), kNavDemoModes, GALLERY_NAV_DEMO_MODES);
	xtk_combo_onselect(&b->txn, xtk_ref_local(combo), page_post, page_bind(b, MSG_NAV_DEMO_MODE));
	page_checkbox(b, row, "Hide Settings", m->nav_demo_hide, MSG_NAV_DEMO_HIDE);
	page_button(b, row, "Select Settings", MSG_NAV_DEMO_PICK, 0);
	page_checkbox(b, row, "Hide page header outside Minimal", m->nav_demo_hide_header, MSG_NAV_DEMO_HIDE_HEADER);
}

bool page_navigation_view_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "NavigationView", "Try the pane modes, Back, and the built-in Settings item.");
	nav_demo_options(b, parent);
	page_sync(b, nav_demo_sync);
	return nav_demo_preview(b, parent);
}
