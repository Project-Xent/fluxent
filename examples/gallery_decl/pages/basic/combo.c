#include "gallery.h"

XtkEl *page_combo(XtkUi *ui, Model const *m) {
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
	int const count      = ( int ) (sizeof(kColors) / sizeof(kColors [0]));
	int const font_count = ( int ) (sizeof(kFonts) / sizeof(kFonts [0]));
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ComboBox", "Use a ComboBox to let the user pick one item from a drop-down list."),
		example(
		  ui, "A ComboBox with items defined inline.",
		  demo_card(
			ui,
			xtk_combo(
			  ui, "Pick a color",
			  (XtkComboDesc) {.items = kColors, .nitem = count, .sel = m->combo_sel, .select = xtk_msg(MSG_COMBO)}
			),
			m->combo_sel >= 0 ? xtk_fmt(ui, "Selected: %s", kColors [m->combo_sel]) : "Nothing selected yet", NULL
		  )
		),
		example(
		  ui, "A ComboBox with a long scrolling list.",
		  demo_card(
			ui,
			xtk_combo(
			  ui, "Pick a font",
			  (XtkComboDesc) {
				.items = kFonts, .nitem = font_count, .sel = m->combo_font, .select = xtk_msg(MSG_COMBO_FONT)}
			),
			m->combo_font >= 0 ? xtk_fmt(ui, "Selected: %s", kFonts [m->combo_font]) : "Nothing selected yet", NULL
		  )
		),
		example(
		  ui, "An editable ComboBox: type to highlight a match, press Enter to submit any text.",
		  demo_card(
			ui,
			xtk_combo(
			  ui, "Type or pick a color",
			  (XtkComboDesc) {
				.items    = kColors,
				.nitem    = count,
				.sel      = m->combo_edit_sel,
				.editable = true,
				.select   = xtk_msg(MSG_COMBO_EDIT),
				.submit   = xtk_msg(MSG_COMBO_SUBMIT)}
			),
			m->combo_edit_sel >= 0 ? xtk_fmt(ui, "Selected: %s", kColors [m->combo_edit_sel])
			: m->combo_submit      ? xtk_fmt(ui, "Submitted: %s", m->combo_submit)
			                       : "Nothing submitted yet",
			NULL
		  )
		)
	  )
	);
}
