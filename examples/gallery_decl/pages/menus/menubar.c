#include "gallery.h"
#include <stdio.h>

XtkEl *page_menus(XtkUi *ui, Model const *m) {
	static char const *const kChoices [] = {
	  "(nothing yet)", "Send email", "Open calendar", "Print", "File > New", "File > Open...", "File > Exit",
	  "Edit > Undo", "Edit > Redo", "Help > About"};
	int choice = (m->menu_choice >= 0 && m->menu_choice <= 9) ? m->menu_choice : 0;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Menus & links", "MenuBar and DropDownButton open flyout menus; RepeatButton fires while held; "
		  "HyperlinkButton opens a URL."
		),
		example(
		  ui, "A MenuBar with File / Edit / Help menus.",
		  xtk_card(
			ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_menubar(
				ui,
				(XtkMenuBarDesc) {
				  .menus =
					(XtkMenuDesc []) {
					  {.title = "File",
				       .items =
				         (XtkMenuItemDesc []) {
						   {.label = "New", .click = xtk_msg_i(MSG_MENU, 4)},
						   {.label = "Open...", .click = xtk_msg_i(MSG_MENU, 5)},
						   {.label = NULL},
						   {.label = "Exit", .click = xtk_msg_i(MSG_MENU, 6)},
						 },
				       .nitem = 4},
					  {.title = "Edit",
				       .items =
				         (XtkMenuItemDesc []) {
						   {.label = "Undo", .icon = "Undo", .click = xtk_msg_i(MSG_MENU, 7)},
						   {.label = "Redo", .icon = "Redo", .click = xtk_msg_i(MSG_MENU, 8)},
						 },
				       .nitem = 2},
					  {.title = "Help",
				       .items = (XtkMenuItemDesc []) {{.label = "About", .click = xtk_msg_i(MSG_MENU, 9)}},
				       .nitem = 1},
					},
				  .nmenu = 3}
			  ))
		  )
		),
		example(
		  ui, "A DropDownButton with a menu.",
		  demo_card(
			ui,
			xtk_dropdown(
			  ui, "Actions",
			  (XtkDropdownDesc) {
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Send email", .icon = "Mail", .click = xtk_msg_i(MSG_MENU, 1)},
					{.label = "Open calendar", .icon = "Forward", .click = xtk_msg_i(MSG_MENU, 2)},
					{.label = NULL},
					{.label = "Print", .icon = "Print", .click = xtk_msg_i(MSG_MENU, 3)},
				  },
				.nitem = 4}
			),
			xtk_fmt(ui, "Last choice: %s", kChoices [choice]), NULL
		  )
		),
		example(
		  ui, "A RepeatButton keeps firing while pressed.",
		  demo_card(
			ui, xtk_repeat(ui, "Click and hold", (XtkButtonDesc) {.click = xtk_msg(MSG_REPEAT)}),
			xtk_fmt(ui, "Fired %d time%s", m->repeats, m->repeats == 1 ? "" : "s"), NULL
		  )
		),
		example(
		  ui, "A HyperlinkButton opens the project page.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_link(
				ui, "FluXent on GitHub", (XtkHyperlinkDesc) {.url = "https://github.com/Project-Xent"}
			  ))
		  )
		))
	);
}
