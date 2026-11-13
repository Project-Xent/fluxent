#include "app.h"

static char const *const kItems [] = {"Option A", "Option B", "Option C"};

static char const       *radio_text(GalleryApp *g) {
	int sel = g->model.radio_buttons_sel;
	return gallery_fmt(g, "Selected: %s", sel >= 0 && sel < 3 ? kItems [sel] : "Nothing selected");
}

static void radio_buttons_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_RADIO_BUTTONS], radio_text(g)); }

bool        page_radio_buttons_build(PageBuild *b) {
	page_sync(b, radio_buttons_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "RadioButtons", "A grouped set of mutually exclusive radio options — one control, one selection index."
	);
	PageDemo   demo  = page_demo(b, page_example(b, parent, "A RadioButtons group with a header."));
	XtkLocalId group = xtk_radiogroup_create(
	  &b->txn, demo.items,
	  &(XtkRadioGroupProps) {
		.set   = XTK_RADIOGROUP_SEL_MASK | XTK_RADIOGROUP_NITEM_MASK | XTK_RADIOGROUP_NCOL_MASK,
		.sel   = b->app->model.radio_buttons_sel,
		.nitem = 3,
		.ncol  = 1,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(group);
	ui_text(&b->txn, ref, "Background", 14.0f, FLUX_FONT_REGULAR);
	for (int i = 0; i < 3; i++)
		xtk_radio_create(&b->txn, ref, &(XtkToggleProps) {.set = XTK_TOGGLE_TEXT_MASK, .text = kItems [i]});
	xtk_radiogroup_onselect(&b->txn, ref, page_post, page_bind(b, MSG_RADIO_BUTTONS));
	page_keepout(b, MSG_RADIO_BUTTONS, page_output(b, demo.row, radio_text(b->app)));
	return true;
}
