#include "app.h"

static char const *result_text(GalleryApp *g) {
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	int                      r           = g->model.dialog_result;
	return gallery_fmt(g, "Last result: %s", r >= 0 && r <= 2 ? kResults [r] : "(none yet)");
}

static void dialog_sync(GalleryApp *g) {
	xtk_dialog_setopen(&g->scene, g->live_control [MSG_DIALOG_OPEN], g->model.dialog_open, (XtkChangeStamp) {0});
	gallery_settext(g, g->live_output [MSG_DIALOG_RESULT], result_text(g));
}

bool page_dialog_build(PageBuild *b) {
	page_sync(b, dialog_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "ContentDialog", "A modal dialog controlled by the model: open while a flag is true.");
	PageDemo demo = page_demo(b, page_example(b, parent, "A dialog with Delete / Cancel buttons."));
	page_button(b, demo.items, "Show dialog", MSG_DIALOG_OPEN, 0);
	page_keepout(b, MSG_DIALOG_RESULT, page_output(b, demo.row, result_text(b->app)));
	XtkLocalId dialog = xtk_dialog_create(
	  &b->txn, parent,
	  &(XtkDialogProps) {
		.set      = XTK_DIALOG_W_MASK
	              | XTK_DIALOG_TEXT_MASK
	              | XTK_DIALOG_PRIMARY_MASK
	              | XTK_DIALOG_CLOSE_MASK
	              | XTK_DIALOG_CONTENTH_MASK
	              | XTK_DIALOG_OPEN_MASK,
		.w        = 400.0f,
		.text     = "Delete file?",
		.primary  = "Delete",
		.close    = "Cancel",
		.contenth = 56.0f,
		.open     = b->app->model.dialog_open,
	  }
	);
	xtk_dialog_onclose(&b->txn, xtk_ref_local(dialog), page_post, page_bind(b, MSG_DIALOG_RESULT));
	ui_text(
	  &b->txn, xtk_ref_local(dialog), "This file will be permanently removed. This cannot be undone.", 14.0f,
	  FLUX_FONT_REGULAR
	);
	page_keep(b, MSG_DIALOG_OPEN, dialog);
	return true;
}
