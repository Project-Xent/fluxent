#include "app.h"

static char const *const kModes [] = {"Inline", "CompactInline", "Overlay", "CompactOverlay"};

static char const       *pane_text(GalleryApp *g) {
	return g->model.split_open ? "The pane is open." : "The pane is collapsed.";
}

static void split_view_sync(GalleryApp *g) {
	XtkCtrl view = g->live_control [MSG_SPLIT_TOGGLE];
	xtk_splitview_setopen(&g->scene, view, g->model.split_open, (XtkChangeStamp) {0});
	xtk_splitview_setdisplay(&g->scene, view, ( uint32_t ) g->model.split_mode, (XtkChangeStamp) {0});
	xtk_splitview_setplacement(
	  &g->scene, view, g->model.split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT, (XtkChangeStamp) {0}
	);
	gallery_settext(g, g->live_output [MSG_SPLIT_TOGGLE], pane_text(g));
}

static void pane_items(XtkTxn *txn, XtkNodeRef pane) {
	static char const *const kItems [][2] = {
	  {"Home",     "Home"     },
	  {"Folder",   "Documents"},
	  {"Pictures", "Pictures" },
	  {"Settings", "Settings" },
	};
	for (int i = 0; i < 4; i++) ui_button(txn, pane, kItems [i][1], kItems [i][0], XTK_BUTTON_SUBTLE);
}

static void mode_row(PageBuild *b, XtkNodeRef parent) {
	Model const *m     = &b->app->model;
	XtkNodeRef   row   = xtk_ref_local(ui_row(&b->txn, parent, 12.0f, true));
	XtkLocalId   combo = xtk_combo_create(
	  &b->txn, row,
	  &(XtkComboProps) {
		.set         = XTK_COMBO_PLACEHOLDER_MASK | XTK_COMBO_SEL_MASK,
		.placeholder = "Display mode",
		.sel         = m->split_mode,
	  }
	);
	xtk_txn_setitems(&b->txn, xtk_ref_local(combo), kModes, 4);
	xtk_combo_onselect(&b->txn, xtk_ref_local(combo), page_post, page_bind(b, MSG_SPLIT_MODE));
	page_checkbox(b, row, "Pane on right", m->split_right, MSG_SPLIT_PLACE);
}

bool page_split_view_build(PageBuild *b) {
	page_sync(b, split_view_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "SplitView",
	  "A collapsible side pane beside main content: Inline and CompactInline push the content; Overlay and "
	  "CompactOverlay float the pane over it."
	);
	mode_row(b, parent);
	XtkLocalId view = xtk_splitview_create(
	  &b->txn, parent,
	  &(XtkSplitViewProps) {
		.set
	    = XTK_SPLITVIEW_H_MASK | XTK_SPLITVIEW_OPEN_MASK | XTK_SPLITVIEW_DISPLAY_MASK | XTK_SPLITVIEW_PLACEMENT_MASK,
		.h         = 320.0f,
		.open      = m->split_open,
		.display   = ( uint32_t ) m->split_mode,
		.placement = m->split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT,
	  }
	);
	XtkNodeRef body    = xtk_ref_local(xtk_splitbody_create(&b->txn, xtk_ref_local(view), NULL));
	XtkNodeRef pane    = xtk_ref_local(xtk_splitpane_create(&b->txn, xtk_ref_local(view), NULL));
	XtkNodeRef content = xtk_ref_local(ui_card(&b->txn, body, 12.0f, XENT_FLEX_ALIGN_START, 16.0f));
	page_button(b, content, "\xE2\x98\xB0 Toggle pane", MSG_SPLIT_TOGGLE, 0);
	ui_text(&b->txn, content, "Main content area", 20.0f, FLUX_FONT_SEMI_BOLD);
	page_keepout(b, MSG_SPLIT_TOGGLE, page_note(b, content, pane_text(b->app)));
	pane_items(&b->txn, xtk_ref_local(ui_column(&b->txn, pane, 4.0f, XENT_FLEX_ALIGN_STRETCH)));
	page_keep(b, MSG_SPLIT_TOGGLE, view);
	return true;
}
