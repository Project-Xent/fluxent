#include "app.h"

static void infobar_sync(GalleryApp *g) {
	XtkTxn txn;
	for (int i = 0; i < BAR_COUNT; i++)
		if (g->model.bar_open [i] && g->live_extra [i] == XTK_CTRL_INVALID) g->rebuild_request = true;
	if (g->rebuild_request || !xtk_txn_begin(&g->scene, &txn)) return;
	for (int i = 0; i < BAR_COUNT; i++) {
		if (g->model.bar_open [i] || g->live_extra [i] == XTK_CTRL_INVALID) continue;
		xtk_txn_drop(&txn, xtk_ref_existing(g->live_extra [i]));
		g->live_extra [i] = XTK_CTRL_INVALID;
	}
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (result) xtk_batchresult_destroy(result);
}

bool page_infobar_build(PageBuild *b) {
	static char const *const kTitles [BAR_COUNT] = {"Informational", "Success", "Warning", "Error"};
	page_sync(b, infobar_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "InfoBar", "Use an InfoBar for essential status messages. Closing one just flips a model flag."
	);
	XtkNodeRef col = xtk_ref_local(ui_column(
	  &b->txn, page_example(b, parent, "One bar per severity; close them, then restore."), 8.0f, XENT_FLEX_ALIGN_STRETCH
	));
	for (int i = 0; i < BAR_COUNT; i++) {
		if (!b->app->model.bar_open [i]) continue;
		XtkLocalId bar = xtk_infobar_create(
		  &b->txn, col,
		  &(XtkInfoBarProps) {
			.set
		    = XTK_INFOBAR_TEXT_MASK | XTK_INFOBAR_MESSAGE_MASK | XTK_INFOBAR_SEVERITY_MASK | XTK_INFOBAR_CLOSABLE_MASK,
			.text     = kTitles [i],
			.message  = "Essential app message; closable like the real control.",
			.severity = ( uint32_t ) i,
			.closable = true,
		  }
		);
		xtk_infobar_onclose(&b->txn, xtk_ref_local(bar), page_post, page_bind_i(b, MSG_BAR_CLOSE, i));
		page_keepx(b, i, bar);
	}
	XtkLocalId reset = page_button(b, col, "Restore all info bars", MSG_BARS_RESET, 0);
	xtk_txn_set(
	  &b->txn, xtk_ref_local(reset), XTK_PROP_ALIGN_SELF,
	  (XtkValue) {.kind = XTK_VALUE_ENUM, .u.u32 = XENT_FLEX_ALIGN_START}, (XtkChangeStamp) {0}
	);
	page_keep(b, MSG_BARS_RESET, reset);
	return true;
}
