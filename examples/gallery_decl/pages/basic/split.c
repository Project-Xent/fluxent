#include "gallery.h"

XtkEl *page_split(XtkUi *ui, Model const *m) {
	static char const *const kActions [] = {"(nothing yet)", "Saved", "Saved as...", "Saved all"};
	int                      action      = (m->split_action >= 0 && m->split_action <= 3) ? m->split_action : 0;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "SplitButton",
		  "The left zone fires the default action; the chevron opens a menu of alternatives."
		),
		example(
		  ui, "A SplitButton for saving with alternatives.",
		  demo_card(
			ui,
			xtk_split(
			  ui, "Save",
			  (XtkSplitDesc) {
				.icon     = "Save",
				.click = xtk_msg(MSG_SPLIT_PRIMARY),
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Save as...", .click = xtk_msg_i(MSG_SPLIT_MENU, 0)},
					{.label = "Save all", .click = xtk_msg_i(MSG_SPLIT_MENU, 1)},
				  },
				.nitem = 2}
			),
			xtk_fmt(ui, "Last action: %s", kActions [action]), NULL
		  )
		))
	);
}
