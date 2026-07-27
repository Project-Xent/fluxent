#include "gallery.h"
#include <string.h>

static char const *const kFonts [] = {
  "Arial", "Calibri", "Cambria", "Consolas", "Courier New", "Georgia", "Segoe UI", "Tahoma", "Times New Roman",
  "Verdana"};

#define FONT_COUNT ( int ) (sizeof(kFonts) / sizeof(kFonts [0]))

static XtkEl *listbox_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, kFonts [index], (XtkTextDesc) {0});
}

XtkEl *page_listbox(XtkUi *ui, Model const *m) {
	char const *status = m->listbox_sel >= 0 ? xtk_fmt(ui, "Font: %s", kFonts [m->listbox_sel]) : "Pick a font.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ListBox", "The bordered list surface with accent-filled selection."),
		example(
		  ui, "Selection fills the row with the accent at WinUI's 0.6/0.8/0.9 opacities.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 16, .wrap = XENT_FLEX_WRAP},
			xtk_kids(
			  xtk_sized(
				xtk_listbox(
				  ui,
				  (XtkListDesc) {
					.nitem  = FONT_COUNT,
					.itemh  = 40.0f,
					.item   = listbox_row,
					.sel    = m->listbox_sel,
					.select = xtk_msg(MSG_LISTBOX_SELECT),
				  }
				),
				260.0f, 320.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13})
			)
		  )
		)
	  )
	);
}
