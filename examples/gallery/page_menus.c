#include "gallery.h"

#include <stdio.h>

FluxEl *page_menus(FxUi *ui, Model const *m) {
	static char const *const kChoices []
	  = {"(nothing yet)", "Send email", "Open calendar", "Print", "File > New", "File > Open...",
	     "File > Exit",   "Edit > Undo", "Edit > Redo",  "Help > About"};
	int choice = (m->menu_choice >= 0 && m->menu_choice <= 9) ? m->menu_choice : 0;
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "Menus & links", "MenuBar and DropDownButton open flyout menus; RepeatButton fires while held; "
		  "HyperlinkButton opens a URL."
		),
		example(
		  ui, "A MenuBar with File / Edit / Help menus.",
		  fx_card(
			ui, (FxStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
			(FluxEl *[]) {
			  fx_menu_bar(
				ui,
				(FxMenuBarDesc) {
				  .menus =
					(FxMenuDesc []) {
					  {.title = "File",
				       .items =
				         (FxMenuItemDesc []) {
						   {.label = "New", .on_click = fx_msg_i(MSG_MENU, 4)},
						   {.label = "Open...", .on_click = fx_msg_i(MSG_MENU, 5)},
						   {.label = NULL},
						   {.label = "Exit", .on_click = fx_msg_i(MSG_MENU, 6)},
						 },
				       .item_count = 4},
					  {.title = "Edit",
				       .items =
				         (FxMenuItemDesc []) {
						   {.label = "Undo", .icon = "Undo", .on_click = fx_msg_i(MSG_MENU, 7)},
						   {.label = "Redo", .icon = "Redo", .on_click = fx_msg_i(MSG_MENU, 8)},
						 },
				       .item_count = 2},
					  {.title = "Help",
				       .items = (FxMenuItemDesc []) {{.label = "About", .on_click = fx_msg_i(MSG_MENU, 9)}},
				       .item_count = 1},
					},
				  .menu_count = 3}
			  ),
			  FX_END}
		  )
		),
		example(
		  ui, "A DropDownButton with a menu.",
		  demo_card(
			ui,
			fx_dropdown_button(
			  ui, "Actions",
			  (FxDropdownDesc) {
				.items =
				  (FxMenuItemDesc []) {
					{.label = "Send email", .icon = "Mail", .on_click = fx_msg_i(MSG_MENU, 1)},
					{.label = "Open calendar", .icon = "Forward", .on_click = fx_msg_i(MSG_MENU, 2)},
					{.label = NULL},
					{.label = "Print", .icon = "Print", .on_click = fx_msg_i(MSG_MENU, 3)},
				  },
				.item_count = 4}
			),
			fx_fmt(ui, "Last choice: %s", kChoices [choice]), NULL
		  )
		),
		example(
		  ui, "A RepeatButton keeps firing while pressed.",
		  demo_card(
			ui, fx_repeat_button(ui, "Click and hold", (FxButtonDesc) {.on_click = fx_msg(MSG_REPEAT)}),
			fx_fmt(ui, "Fired %d time%s", m->repeats, m->repeats == 1 ? "" : "s"), NULL
		  )
		),
		example(
		  ui, "A HyperlinkButton opens the project page.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_hyperlink(
				ui, "FluXent on GitHub", (FxHyperlinkDesc) {.url = "https://github.com/Project-Xent"}
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}

FluxEl *page_tabview(FxUi *ui, Model const *m) {
	FxTabDesc tabs [8];
	int       count = m->tab_count;
	for (int i = 0; i < count; i++)
		tabs [i] = (FxTabDesc) {.icon = "OpenFile", .label = fx_fmt(ui, "Document %d", m->tab_ids [i])};
	int sel = (m->tab_sel >= 0 && m->tab_sel < count) ? m->tab_sel : 0;

	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "TabView",
		  "Tabs live in the model: closing posts a message and the keyed element remounts with the new set."
		),
		example(
		  ui, "A TabView with closable tabs and an add button.",
		  fx_keyed(
			ui, fx_fmt(ui, "tabs-%d", m->tab_gen),
			fx_sized(
			  fx_tab_view(
				ui,
				(FxTabViewDesc) {
				  .tabs      = tabs,
				  .tab_count = count,
				  .selected  = sel,
				  .on_select = fx_msg(MSG_TAB_SELECT),
				  .on_close  = fx_msg(MSG_TAB_CLOSE),
				  .on_add    = fx_msg(MSG_TAB_ADD)},
				(FluxEl *[]) {
				  fx_column(
					ui,
					(FxStackDesc) {
					  .gap = 8, .padding = {16, 16, 16, 16}
                },
					(FluxEl *[]) {
					  fx_text(
						ui, fx_fmt(ui, "Document %d", count ? m->tab_ids [sel] : 0),
						(FxTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}
					  ),
					  fx_text(
						ui, "This page is a function of the model's selection — the same fx subtree follows it.",
						(FxTextDesc) {.size = 13}
					  ),
					  FX_END}
				  ),
				  FX_END}
			  ),
			  NAN, 280
			)
		  )
		),
		FX_END}
	);
}

FluxEl *page_expander(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "Expander", "An Expander shows a header and reveals a content region when toggled."),
		example(
		  ui, "An Expander with interactive content.",
		  fx_expander(
			ui, "This Expander is bound to the model",
			(FxExpanderDesc) {.expanded = m->expanded, .on_toggle = fx_msg(MSG_EXPAND)},
			(FluxEl *[]) {
			  fx_text(ui, "Anything can live in here — it is a normal FX container.", (FxTextDesc) {0}),
			  fx_checkbox(
				ui, "Even this checkbox from the CheckBox page",
				(FxToggleDesc) {.checked = m->check_a, .on_change = fx_msg(MSG_CHECK_A)}
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}
