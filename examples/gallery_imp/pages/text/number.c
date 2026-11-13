#include "app.h"
#include <string.h>

static void page_numberbox_sync(GalleryApp *g);

bool        page_numberbox_build(PageBuild *b) {
	page_sync(b, page_numberbox_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "NumberBox", "Use a NumberBox for numeric input with spin buttons, wheel, and keyboard support."
	);
	XtkNodeRef ex     = page_example(b, parent, "A NumberBox with spin buttons (0–100).");
	PageDemo   demo   = page_demo(b, ex);
	XtkLocalId number = xtk_number_create(
	  &b->txn, demo.items,
	  &(XtkNumberProps) {
		.set     = XTK_NUMBER_VALUE_MASK
	             | XTK_NUMBER_MIN_MASK
	             | XTK_NUMBER_MAX_MASK
	             | XTK_NUMBER_STEP_MASK
	             | XTK_NUMBER_ENABLED_MASK,
		.value   = m->quantity,
		.min     = 0.0,
		.max     = 100.0,
		.step    = 1.0,
		.enabled = true,
	  }
	);
	xtk_number_onchange(&b->txn, xtk_ref_local(number), page_post, page_bind(b, MSG_QUANTITY));
	page_keep(b, MSG_QUANTITY, number);
	XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "Quantity: %.0f", m->quantity));
	page_keepout(b, MSG_QUANTITY, output);
	return true;
}

static void page_numberbox_sync(GalleryApp *g) {
	char const *text = gallery_fmt(g, "Quantity: %.0f", g->model.quantity);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_QUANTITY], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}
