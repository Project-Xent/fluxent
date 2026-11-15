#include "app.h"

static char const *last_command(GalleryApp *g) {
	int last = g->model.cb_last;
	return last >= 0 ? gallery_fmt(g, "You clicked: %s", kCbCommands [last]) : "Click a command.";
}

static void commandbar_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_CB_ACTION], last_command(g)); }

static void command(PageBuild *b, XtkNodeRef bar, int i, bool secondary, char const *accel) {
	uint64_t   set   = XTK_APPBARBUTTON_TEXT_MASK | XTK_APPBARBUTTON_ICON_MASK | XTK_APPBARBUTTON_SECONDARY_MASK;
	XtkLocalId local = xtk_appbarbutton_create(
	  &b->txn, bar,
	  &(XtkAppBarButtonProps) {
		.set       = set | (accel ? XTK_APPBARBUTTON_ACCEL_MASK : 0u),
		.text      = kCbCommands [i],
		.icon      = kCbCommands [i],
		.accel     = accel,
		.secondary = secondary,
	  }
	);
	xtk_appbarbutton_oninvoke(&b->txn, xtk_ref_local(local), page_post, page_bind_i(b, MSG_CB_ACTION, i));
}

static void toggle(PageBuild *b, XtkNodeRef bar, char const *text, char const *icon, bool secondary, int tag) {
	XtkLocalId local = xtk_appbartoggle_create(
	  &b->txn, bar,
	  &(XtkAppBarToggleProps) {
		.set       = XTK_APPBARTOGGLE_TEXT_MASK | XTK_APPBARTOGGLE_ICON_MASK | XTK_APPBARTOGGLE_SECONDARY_MASK,
		.text      = text,
		.icon      = icon,
		.secondary = secondary,
	  }
	);
	xtk_appbartoggle_onchange(&b->txn, xtk_ref_local(local), page_post, page_bind(b, tag));
}

static void separator(PageBuild *b, XtkNodeRef bar) {
	( void ) xtk_appbarsep_create(&b->txn, bar, &(XtkAppBarSeparatorProps) {0});
}

static XtkNodeRef bar_card(PageBuild *b, XtkNodeRef parent, char const *caption, XtkCommandBarProps const *props) {
	XtkNodeRef card
	  = xtk_ref_local(ui_card(&b->txn, page_example(b, parent, caption), 12.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f));
	XtkLocalId bar = xtk_commandbar_create(&b->txn, card, props);
	xtk_commandbar_onopen(&b->txn, xtk_ref_local(bar), page_post, page_bind(b, MSG_CB_OPEN));
	return xtk_ref_local(bar);
}

bool page_commandbar_build(PageBuild *b) {
	page_sync(b, commandbar_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "CommandBar", "A toolbar for primary commands, with secondary commands in an overflow menu."
	);
	XtkNodeRef bar = bar_card(
	  b, parent, "Labels show when the bar is open; secondary commands go to the overflow menu.",
	  &(XtkCommandBarProps) {0}
	);
	for (int i = 0; i < 3; i++) command(b, bar, i, false, NULL);
	separator(b, bar);
	toggle(b, bar, "Favorite", "Favorite", false, MSG_CB_FAV);
	command(b, bar, 8, true, "Ctrl+I");
	command(b, bar, 9, true, "Ctrl+P");
	bar = bar_card(
	  b, parent, "Labels on the right with dynamic overflow. Narrow the window to move commands into the menu.",
	  &(XtkCommandBarProps) {
		.set     = XTK_COMMANDBAR_LABELS_MASK | XTK_COMMANDBAR_DYNAMIC_MASK,
		.labels  = XTK_LABEL_RIGHT,
		.dynamic = true,
	  }
	);
	for (int i = 0; i < 4; i++) command(b, bar, i, false, NULL);
	separator(b, bar);
	for (int i = 4; i < 8; i++) command(b, bar, i, false, NULL);
	toggle(b, bar, "Bold", "Font", true, MSG_CB_BOLD);
	command(b, bar, 8, true, "Ctrl+I");
	bar = bar_card(
	  b, parent, "Minimal closed display mode.",
	  &(XtkCommandBarProps) {.set = XTK_COMMANDBAR_MODE_MASK, .mode = XTK_APPBAR_MINIMAL}
	);
	command(b, bar, 0, false, NULL);
	command(b, bar, 1, false, NULL);
	command(b, bar, 9, true, "Ctrl+P");
	page_keepout(b, MSG_CB_ACTION, page_note(b, parent, last_command(b->app)));
	return true;
}
