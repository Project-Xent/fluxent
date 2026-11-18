#include "app.h"

/*
 * System backdrop materials, imperative build.
 *
 * Same content as the declarative page: DWM backdrops applied to the real window, and the
 * see-through vs painted comparison. Popups are shown but stay opaque: a popup is a tool window
 * without a DWM redirection surface, so it cannot take the system backdrop.
 */

static char const *const kNames [] = {"None", "Mica", "Mica Alt", "Acrylic"};

static char const *const kDwm []
  = {"DWMSBT_NONE", "DWMSBT_MAINWINDOW", "DWMSBT_TABBEDWINDOW", "DWMSBT_TRANSIENTWINDOW"};

static char const *const kWhat [] = {
  "No material: the window paints solid_background and is fully opaque.",
  "Mica: the desktop wallpaper is blurred behind the window.",
  "Mica Alt (Tabbed): Mica with a stronger tint, for tabbed shells.",
  "Acrylic: a live blur of whatever sits behind the window.",
};

static char const *const kPopupItems [] = {"First item", "Second item", "Third item"};

#define BACKDROP_COUNT ( int ) (sizeof(kNames) / sizeof(kNames [0]))

static int backdrop_cur(GalleryApp *g) {
	int v = g->model.backdrop;
	return (v >= 0 && v < BACKDROP_COUNT) ? v : 0;
}

static char const *backdrop_text(GalleryApp *g) {
	int cur = backdrop_cur(g);
	return gallery_fmt(g, "Active: %s  (%s)\n%s", kNames [cur], kDwm [cur], kWhat [cur]);
}

static void materials_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_BACKDROP], backdrop_text(g)); }

bool        page_materials_build(PageBuild *b) {
	page_sync(b, materials_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);

	page_header(
	  b, parent, "Materials",
	  "System backdrops (Mica, Mica Alt, Acrylic) applied to the real window. These are DWM "
	  "materials: Fluxent asks for one, then keeps the client area transparent wherever nothing "
	  "is painted."
	);

	{
		PageDemo demo = page_demo(b, page_example(b, parent, "Backdrop"));
		for (int i = 0; i < BACKDROP_COUNT; i++) {
			XtkLocalId btn = ui_button(
			  &b->txn, demo.items, kNames [i], NULL,
			  b->app->model.backdrop == i ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD
			);
			xtk_button_oninvoke(&b->txn, xtk_ref_local(btn), page_post, page_bind_i(b, MSG_BACKDROP, i));
		}
		page_keepout(b, MSG_BACKDROP, page_output(b, demo.row, backdrop_text(b->app)));
	}

	{
		XtkNodeRef ex  = page_example(b, parent, "See-through vs painted");
		XtkLocalId row = ui_row(&b->txn, ex, 12.0f, false);
		XtkLocalId a   = xtk_container_create(
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
		ui_text(&b->txn, xtk_ref_local(a), "Bare page", 14.0f, FLUX_FONT_SEMI_BOLD);
		ui_text(
		  &b->txn, xtk_ref_local(a), "Nothing paints here - the backdrop is the background.", 12.0f, FLUX_FONT_REGULAR
		);

		XtkLocalId c = xtk_card_create(
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
		ui_text(&b->txn, xtk_ref_local(c), "Card", 14.0f, FLUX_FONT_SEMI_BOLD);
		ui_text(
		  &b->txn, xtk_ref_local(c), "Cards paint card_bg_default, which hides the material.", 12.0f, FLUX_FONT_REGULAR
		);
	}

	{
		XtkNodeRef ex = page_example(b, parent, "Popup surface");
		XtkLocalId cb = xtk_combo_create(
		  &b->txn, ex,
		  &(XtkComboProps) {
			.set         = XTK_COMBO_PLACEHOLDER_MASK | XTK_COMBO_SEL_MASK,
			.placeholder = "Open the drop-down popup",
			.sel         = 0,
		  }
		);
		xtk_txn_setitems(&b->txn, xtk_ref_local(cb), kPopupItems, 3);
		ui_text(
		  &b->txn, ex,
		  "The drop-down is a real OS popup. A popup cannot take the system backdrop: it is a tool "
		  "window without a DWM redirection surface, so the acrylic brush has nothing to blur there. "
		  "It stays opaque by design.",
		  12.0f, FLUX_FONT_REGULAR
		);
	}

	{
		XtkNodeRef ex = page_example(b, parent, "Popups and the system backdrop");
		ui_text(
		  &b->txn, ex,
		  "The system backdrop belongs to the window, not to elements inside it. An element that "
		  "wants a material of its own needs a backdrop brush behind it, which the Composition "
		  "renderer exposes and the Direct2D renderer does not; the window material above is what "
		  "both renderers honour.",
		  12.0f, FLUX_FONT_REGULAR
		);
	}

	ui_text(
	  &b->txn, parent,
	  "Requires Windows 11 22H2 (build 22621) or newer; below that Fluxent clamps the backdrop to "
	  "None. Your choice is written to fluxent-gallery.ini under LOCALAPPDATA and restored on the "
	  "next run.",
	  12.0f, FLUX_FONT_REGULAR
	);
	return true;
}
