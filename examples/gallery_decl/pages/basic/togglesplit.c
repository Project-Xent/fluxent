#include "gallery.h"

XtkEl *page_toggle_split(XtkUi *ui, Model const *m) {
	static char const *const kMenuOut [] = {"(primary click)", "Align left", "Align center", "Align right"};
	int                      menu = m->toggle_split_menu >= 0 && m->toggle_split_menu <= 3 ? m->toggle_split_menu : 0;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ToggleSplitButton",
		  "The left zone toggles checked state; the chevron opens a menu of secondary actions."
		),
		example(
		  ui, "Toggle + menu split button.",
		  demo_card(
			ui,
			xtk_togglesplit(
			  ui, "Bold",
			  (XtkSplitDesc) {
				.icon      = "Bold",
				.checked   = m->toggle_split_checked,
				.toggle = xtk_msg(MSG_TOGGLE_SPLIT),
				.click  = xtk_msg(MSG_TOGGLE_SPLIT_CLICK),
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Align left", .click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 0)},
					{.label = "Align center", .click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 1)},
					{.label = "Align right", .click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 2)},
				  },
				.nitem = 3,
			      }
			),
			xtk_fmt(ui, "%s — %s", m->toggle_split_checked ? "On" : "Off", kMenuOut [menu]), NULL
		  )
		))
	);
}
