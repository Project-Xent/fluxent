#include "app.h"

static char const *const kNames [] = {"None", "Mica", "Mica Alt", "Acrylic"};

static char const *const kDwm []
  = {"DWMSBT_NONE", "DWMSBT_MAINWINDOW", "DWMSBT_TABBEDWINDOW", "DWMSBT_TRANSIENTWINDOW"};

static char const *const kWhat [] = {
  "No material: the window paints solid_background and is fully opaque.",
  "Mica: the desktop wallpaper is blurred behind the window.",
  "Mica Alt (Tabbed): Mica with a stronger tint, for tabbed shells.",
  "Acrylic: a live blur of whatever sits behind the window.",
};

#define BACKDROP_COUNT ( int ) (sizeof(kNames) / sizeof(kNames [0]))

typedef XtkLocalId (*PatchFn)(XtkTxn *txn, XtkNodeRef parent, XtkContainerProps const *props);

static void        patch(PageBuild *b, XtkLocalId row, PatchFn make, char const *caption, char const *note) {
	XtkLocalId p = make(
	  &b->txn, xtk_ref_local(row),
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK
	           | XTK_CONTAINER_GROW_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_START,
		.gap   = 6.0f,
		.pl    = 16.0f,
		.pt    = 14.0f,
		.pr    = 16.0f,
		.pb    = 14.0f,
		.grow  = 1.0f,
	  }
	);
	ui_text(&b->txn, xtk_ref_local(p), caption, 14.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(&b->txn, xtk_ref_local(p), note, 12.0f, FLUX_FONT_REGULAR);
}

bool page_materials_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	int        cur    = b->app->model.backdrop;
	if (cur < 0 || cur >= BACKDROP_COUNT) cur = 0;

	page_header(
	  b, parent, "Materials",
	  "System backdrops (Mica, Mica Alt, Acrylic) applied to the real window. These are DWM "
	  "materials: Fluxent asks for one, then keeps the client area transparent wherever nothing "
	  "is painted."
	);

	PageDemo demo = page_demo(b, page_example(b, parent, "Backdrop"));
	for (int i = 0; i < BACKDROP_COUNT; i++) {
		XtkLocalId btn
		  = ui_button(&b->txn, demo.items, kNames [i], NULL, cur == i ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD);
		xtk_button_oninvoke(&b->txn, xtk_ref_local(btn), page_post, page_bind_i(b, MSG_BACKDROP, i));
	}
	page_output(b, demo.row, gallery_fmt(b->app, "Active: %s  (%s)\n%s", kNames [cur], kDwm [cur], kWhat [cur]));

	XtkLocalId row = ui_row(&b->txn, page_example(b, parent, "See-through vs painted"), 12.0f, false);
	patch(b, row, xtk_container_create, "Bare page", "Nothing paints here - the backdrop is the background.");
	patch(b, row, xtk_card_create, "Card", "Cards paint card_bg_default, which hides the material.");

	ui_text(
	  &b->txn, parent,
	  "The backdrop belongs to the window, not to elements inside it: anything that paints an "
	  "opaque background covers it. Requires Windows 11 22H2 (build 22621) or newer; below that "
	  "Fluxent clamps the backdrop to None. Your choice is written to fluxent-gallery.ini under "
	  "LOCALAPPDATA and restored on the next run.",
	  12.0f, FLUX_FONT_REGULAR
	);
	return true;
}
