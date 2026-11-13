#include "app.h"

static char const *const kColors [] = {"Blue", "Green", "Red", "Yellow"};

static char const *const kFonts []  = {
  "Arial",
  "Bahnschrift",
  "Cambria",
  "Candara",
  "Comic Sans MS",
  "Consolas",
  "Constantia",
  "Corbel",
  "Courier New",
  "Ebrima",
  "Gabriola",
  "Gadugi",
  "Georgia",
  "Impact",
  "Lucida Console",
  "MS Gothic",
  "MV Boli",
  "Marlett",
  "Segoe Print",
  "Segoe Script",
  "Segoe UI",
  "SimSun",
  "Sylfaen",
  "Symbol",
  "Tahoma",
  "Times New Roman",
  "Trebuchet MS",
  "Verdana",
};

#define COLOR_COUNT ( int ) (sizeof(kColors) / sizeof(kColors [0]))
#define FONT_COUNT  ( int ) (sizeof(kFonts) / sizeof(kFonts [0]))

static char const *picked(GalleryApp *g, char const *const *items, int sel) {
	return sel >= 0 ? gallery_fmt(g, "Selected: %s", items [sel]) : "Nothing selected yet";
}

static char const *submitted(GalleryApp *g) {
	if (g->model.combo_edit_sel >= 0) return gallery_fmt(g, "Selected: %s", kColors [g->model.combo_edit_sel]);
	return g->model.combo_submit ? gallery_fmt(g, "Submitted: %s", g->model.combo_submit) : "Nothing submitted yet";
}

static void combo_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_COMBO], picked(g, kColors, g->model.combo_sel));
	gallery_settext(g, g->live_output [MSG_COMBO_FONT], picked(g, kFonts, g->model.combo_font));
	gallery_settext(g, g->live_output [MSG_COMBO_SUBMIT], submitted(g));
}

static XtkLocalId combo(
  PageBuild *b, XtkNodeRef parent, char const *placeholder, char const *const *items, int count, int sel, bool editable
) {
	XtkLocalId local = xtk_combo_create(
	  &b->txn, parent,
	  &(XtkComboProps) {
		.set         = XTK_COMBO_PLACEHOLDER_MASK | XTK_COMBO_SEL_MASK | XTK_COMBO_EDITABLE_MASK,
		.placeholder = placeholder,
		.sel         = sel,
		.editable    = editable,
	  }
	);
	xtk_txn_setitems(&b->txn, xtk_ref_local(local), items, count);
	return local;
}

bool page_combo_build(PageBuild *b) {
	page_sync(b, combo_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(b, parent, "ComboBox", "Use a ComboBox to let the user pick one item from a drop-down list.");
	{
		PageDemo   demo = page_demo(b, page_example(b, parent, "A ComboBox with items defined inline."));
		XtkLocalId cb   = combo(b, demo.items, "Pick a color", kColors, COLOR_COUNT, m->combo_sel, false);
		xtk_combo_onselect(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_COMBO));
		page_keepout(b, MSG_COMBO, page_output(b, demo.row, picked(b->app, kColors, m->combo_sel)));
	}
	{
		PageDemo   demo = page_demo(b, page_example(b, parent, "A ComboBox with a long scrolling list."));
		XtkLocalId cb   = combo(b, demo.items, "Pick a font", kFonts, FONT_COUNT, m->combo_font, false);
		xtk_combo_onselect(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_COMBO_FONT));
		page_keepout(b, MSG_COMBO_FONT, page_output(b, demo.row, picked(b->app, kFonts, m->combo_font)));
	}
	{
		PageDemo demo = page_demo(
		  b, page_example(b, parent, "An editable ComboBox: type to highlight a match, press Enter to submit any text.")
		);
		XtkLocalId cb = combo(b, demo.items, "Type or pick a color", kColors, COLOR_COUNT, m->combo_edit_sel, true);
		xtk_combo_onselect(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_COMBO_EDIT));
		xtk_combo_onsubmit(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_COMBO_SUBMIT));
		page_keepout(b, MSG_COMBO_SUBMIT, page_output(b, demo.row, submitted(b->app)));
	}
	return true;
}
