#include "gallery.h"

static XtkEl *command(XtkUi *ui, int i, bool secondary, char const *accel) {
	return xtk_appbarbutton(
	  ui, kCbCommands [i],
	  (XtkAppBarDesc) {
		.icon      = kCbCommands [i],
		.accel     = accel,
		.secondary = secondary,
		.click     = xtk_msg_i(MSG_CB_ACTION, i),
	  }
	);
}

static char const *last_command(XtkUi *ui, Model const *m) {
	return m->cb_last >= 0 ? xtk_fmt(ui, "You clicked: %s", kCbCommands [m->cb_last]) : "Click a command.";
}

static XtkEl *labeled_bar(XtkUi *ui, Model const *m) {
	return xtk_commandbar(
	  ui, (XtkCommandBarDesc) {.open = m->cb_open, .change = xtk_msg(MSG_CB_OPEN)},
	  xtk_kids(
		command(ui, 0, false, NULL), command(ui, 1, false, NULL), command(ui, 2, false, NULL),
		xtk_appbarsep(ui, (XtkAppBarDesc) {0}),
		xtk_appbartoggle(
		  ui, "Favorite", (XtkAppBarDesc) {.icon = "Favorite", .checked = m->cb_fav, .change = xtk_msg(MSG_CB_FAV)}
		),
		command(ui, 8, true, "Ctrl+I"), command(ui, 9, true, "Ctrl+P")
	  )
	);
}

static XtkEl *dynamic_bar(XtkUi *ui, Model const *m) {
	return xtk_commandbar(
	  ui, (XtkCommandBarDesc) {.labels = XTK_LABEL_RIGHT, .dynamic = true},
	  xtk_kids(
		command(ui, 0, false, NULL), command(ui, 1, false, NULL), command(ui, 2, false, NULL),
		command(ui, 3, false, NULL), xtk_appbarsep(ui, (XtkAppBarDesc) {0}), command(ui, 4, false, NULL),
		command(ui, 5, false, NULL), command(ui, 6, false, NULL), command(ui, 7, false, NULL),
		xtk_appbartoggle(
		  ui, "Bold",
		  (XtkAppBarDesc) {.icon = "Font", .checked = m->cb_bold, .secondary = true, .change = xtk_msg(MSG_CB_BOLD)}
		),
		command(ui, 8, true, "Ctrl+I")
	  )
	);
}

static XtkEl *minimal_bar(XtkUi *ui) {
	return xtk_commandbar(
	  ui, (XtkCommandBarDesc) {.mode = XTK_APPBAR_MINIMAL},
	  xtk_kids(command(ui, 0, false, NULL), command(ui, 1, false, NULL), command(ui, 9, true, "Ctrl+P"))
	);
}

static XtkEl *bar_card(XtkUi *ui, XtkEl *bar, char const *status) {
	return xtk_card(
	  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(xtk_text(ui, status, (XtkTextDesc) {.size = 14}), bar)
	);
}

XtkEl *page_commandbar(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "CommandBar", "A toolbar for primary commands, with secondary commands in an overflow menu."),
		example(
		  ui, "Labels show when the bar is open; secondary commands go to the overflow menu.",
		  bar_card(ui, labeled_bar(ui, m), last_command(ui, m))
		),
		example(
		  ui, "Labels on the right with dynamic overflow. Narrow the window to move commands into the menu.",
		  bar_card(ui, dynamic_bar(ui, m), last_command(ui, m))
		),
		example(ui, "Minimal closed display mode.", bar_card(ui, minimal_bar(ui), last_command(ui, m)))
	  )
	);
}
