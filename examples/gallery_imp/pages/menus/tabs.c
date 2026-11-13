#include "app.h"

#include <stdio.h>

static int  tab_sel(Model const *m) { return m->tab_sel >= 0 && m->tab_sel < m->ntab ? m->tab_sel : 0; }

static void tab_items(GalleryApp *g, XtkTxn *txn, XtkNodeRef tabs) {
	static char  labels [8][24];
	XtkTabDesc   items [8];
	Model const *m = &g->model;
	for (int i = 0; i < m->ntab; i++) {
		snprintf(labels [i], sizeof(labels [i]), "Document %d", m->tab_ids [i]);
		items [i] = (XtkTabDesc) {.icon = "OpenFile", .label = labels [i]};
	}
	xtk_txn_settabs(txn, tabs, items, m->ntab);
	xtk_txn_set(
	  txn, tabs, XTK_PROP_ITEM_COUNT, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = m->ntab}, (XtkChangeStamp) {0}
	);
	xtk_txn_set(
	  txn, tabs, XTK_PROP_SELECTED_INDEX, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = tab_sel(m)}, (XtkChangeStamp) {0}
	);
}

static XtkLocalId tab_content(GalleryApp *g, XtkTxn *txn, XtkNodeRef tabs) {
	Model const *m = &g->model;
	if (m->ntab == 0) return XTK_LOCAL_INVALID;
	XtkLocalId col = ui_card(txn, tabs, 8.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f);
	ui_text(
	  txn, xtk_ref_local(col), gallery_fmt(g, "Document %d", m->tab_ids [tab_sel(m)]), 20.0f, FLUX_FONT_SEMI_BOLD
	);
	ui_text(
	  txn, xtk_ref_local(col), "This page is a function of the model's selection — the same view subtree follows it.",
	  13.0f, FLUX_FONT_REGULAR
	);
	return col;
}

static void tabview_sync(GalleryApp *g) {
	XtkTxn txn;
	if (!xtk_txn_begin(&g->scene, &txn)) return;
	XtkNodeRef tabs = xtk_ref_existing(g->live_control [MSG_TAB_SELECT]);
	tab_items(g, &txn, tabs);
	if (g->live_extra [0] != XTK_CTRL_INVALID) xtk_txn_drop(&txn, xtk_ref_existing(g->live_extra [0]));
	XtkLocalId      content = tab_content(g, &txn, tabs);
	XtkBatchResult *result  = xtk_txn_commit(&txn, NULL);
	if (!result) return;
	g->live_extra [0] = XTK_CTRL_INVALID;
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == content) g->live_extra [0] = result->controls [i].control;
	xtk_batchresult_destroy(result);
}

bool page_tabview_build(PageBuild *b) {
	page_sync(b, tabview_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "TabView", "Tabs live in the model: selection, close, and add update the retained control.");
	XtkNodeRef ex   = page_example(b, parent, "A TabView with closable tabs and an add button.");
	XtkLocalId tabs = xtk_tabs_create(&b->txn, ex, &(XtkSelectionProps) {.set = XTK_SELECTION_H_MASK, .h = 280.0f});
	XtkNodeRef ref  = xtk_ref_local(tabs);
	tab_items(b->app, &b->txn, ref);
	xtk_txn_set(
	  &b->txn, ref, XTK_PROP_ADD_VISIBLE, (XtkValue) {.kind = XTK_VALUE_BOOL, .u.b = true}, (XtkChangeStamp) {0}
	);
	xtk_tabs_onselect(&b->txn, ref, page_post, page_bind(b, MSG_TAB_SELECT));
	page_on(b, tabs, XTK_EVENT_CLOSE, MSG_TAB_CLOSE, 0);
	xtk_tabs_oninvoke(&b->txn, ref, page_post, page_bind(b, MSG_TAB_ADD));
	page_keep(b, MSG_TAB_SELECT, tabs);
	page_keepx(b, 0, tab_content(b->app, &b->txn, ref));
	return true;
}
