#include "gallery.h"

#include <stdio.h>

XtkEl *page_menus(XtkUi *ui, Model const *m) {
	static char const *const kChoices []
	  = {"(nothing yet)", "Send email", "Open calendar", "Print", "File > New", "File > Open...",
	     "File > Exit",   "Edit > Undo", "Edit > Redo",  "Help > About"};
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
			  xtk_menu_bar(
				ui,
				(XtkMenuBarDesc) {
				  .menus =
					(XtkMenuDesc []) {
					  {.title = "File",
				       .items =
				         (XtkMenuItemDesc []) {
						   {.label = "New", .on_click = xtk_msg_i(MSG_MENU, 4)},
						   {.label = "Open...", .on_click = xtk_msg_i(MSG_MENU, 5)},
						   {.label = NULL},
						   {.label = "Exit", .on_click = xtk_msg_i(MSG_MENU, 6)},
						 },
				       .item_count = 4},
					  {.title = "Edit",
				       .items =
				         (XtkMenuItemDesc []) {
						   {.label = "Undo", .icon = "Undo", .on_click = xtk_msg_i(MSG_MENU, 7)},
						   {.label = "Redo", .icon = "Redo", .on_click = xtk_msg_i(MSG_MENU, 8)},
						 },
				       .item_count = 2},
					  {.title = "Help",
				       .items = (XtkMenuItemDesc []) {{.label = "About", .on_click = xtk_msg_i(MSG_MENU, 9)}},
				       .item_count = 1},
					},
				  .menu_count = 3}
			  ))
		  )
		),
		example(
		  ui, "A DropDownButton with a menu.",
		  demo_card(
			ui,
			xtk_dropdown_button(
			  ui, "Actions",
			  (XtkDropdownDesc) {
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Send email", .icon = "Mail", .on_click = xtk_msg_i(MSG_MENU, 1)},
					{.label = "Open calendar", .icon = "Forward", .on_click = xtk_msg_i(MSG_MENU, 2)},
					{.label = NULL},
					{.label = "Print", .icon = "Print", .on_click = xtk_msg_i(MSG_MENU, 3)},
				  },
				.item_count = 4}
			),
			xtk_fmt(ui, "Last choice: %s", kChoices [choice]), NULL
		  )
		),
		example(
		  ui, "A RepeatButton keeps firing while pressed.",
		  demo_card(
			ui, xtk_repeat_button(ui, "Click and hold", (XtkButtonDesc) {.on_click = xtk_msg(MSG_REPEAT)}),
			xtk_fmt(ui, "Fired %d time%s", m->repeats, m->repeats == 1 ? "" : "s"), NULL
		  )
		),
		example(
		  ui, "A HyperlinkButton opens the project page.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_hyperlink(
				ui, "FluXent on GitHub", (XtkHyperlinkDesc) {.url = "https://github.com/Project-Xent"}
			  ))
		  )
		))
	);
}

XtkEl *page_tabview(XtkUi *ui, Model const *m) {
	XtkTabDesc tabs [8];
	int       count = m->tab_count;
	for (int i = 0; i < count; i++)
		tabs [i] = (XtkTabDesc) {.icon = "OpenFile", .label = xtk_fmt(ui, "Document %d", m->tab_ids [i])};
	int sel = (m->tab_sel >= 0 && m->tab_sel < count) ? m->tab_sel : 0;

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "TabView",
		  "Tabs live in the model: closing posts a message and the keyed element remounts with the new set."
		),
		example(
		  ui, "A TabView with closable tabs and an add button.",
		  xtk_keyed(
			ui, xtk_fmt(ui, "tabs-%d", m->tab_gen),
			xtk_sized(
			  xtk_tab_view(
				ui,
				(XtkTabViewDesc) {
				  .tabs      = tabs,
				  .tab_count = count,
				  .selected  = sel,
				  .on_select = xtk_msg(MSG_TAB_SELECT),
				  .on_close  = xtk_msg(MSG_TAB_CLOSE),
				  .on_add    = xtk_msg(MSG_TAB_ADD)},
				xtk_kids(
				  xtk_column(
					ui,
					(XtkStackDesc) {
					  .gap = 8, .padding = {16, 16, 16, 16}
                },
					xtk_kids(
					  xtk_text(
						ui, xtk_fmt(ui, "Document %d", count ? m->tab_ids [sel] : 0),
						(XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}
					  ),
					  xtk_text(
						ui, "This page is a function of the model's selection — the same view subtree follows it.",
						(XtkTextDesc) {.size = 13}
					  ))
				  ))
			  ),
			  NAN, 280
			)
		  )
		))
	);
}

XtkEl *page_expander(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Expander", "An Expander shows a header and reveals a content region when toggled."),
		example(
		  ui, "An Expander with interactive content.",
		  xtk_expander(
			ui, "This Expander is bound to the model",
			(XtkExpanderDesc) {.expanded = m->expanded, .on_toggle = xtk_msg(MSG_EXPAND)},
			xtk_kids(
			  xtk_text(ui, "Anything can live in here — it is a normal container.", (XtkTextDesc) {0}),
			  xtk_checkbox(
				ui, "Even this checkbox from the CheckBox page",
				(XtkToggleDesc) {.checked = m->check_a, .on_change = xtk_msg(MSG_CHECK_A)}
			  ))
		  )
		))
	);
}
