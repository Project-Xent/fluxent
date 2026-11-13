#include "app.h"

static char const *const kChoices [] = {
  "(nothing yet)", "Send email", "Open calendar", "Print", "File > New", "File > Open...", "File > Exit", "Edit > Undo",
  "Edit > Redo", "Help > About"};

typedef struct MenuEntry {
	char const *text;
	char const *icon;
	int         choice;
} MenuEntry;

static char const *choice_text(GalleryApp *g) {
	int choice = g->model.menu_choice >= 0 && g->model.menu_choice <= 9 ? g->model.menu_choice : 0;
	return gallery_fmt(g, "Last choice: %s", kChoices [choice]);
}

static char const *repeat_text(GalleryApp *g) {
	return gallery_fmt(g, "Fired %d time%s", g->model.repeats, g->model.repeats == 1 ? "" : "s");
}

static void menus_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_MENU], choice_text(g));
	gallery_settext(g, g->live_output [MSG_REPEAT], repeat_text(g));
}

static void menu_entries(PageBuild *b, XtkNodeRef menu, MenuEntry const *entries, int count) {
	for (int i = 0; i < count; i++) {
		XtkLocalId item = xtk_menuitem_create(
		  &b->txn, menu,
		  &(XtkItemProps) {
			.set  = (entries [i].text ? XTK_ITEM_TEXT_MASK : 0u) | (entries [i].icon ? XTK_ITEM_ICON_MASK : 0u),
			.text = entries [i].text,
			.icon = entries [i].icon,
		  }
		);
		if (entries [i].text)
			xtk_menuitem_oninvoke(
			  &b->txn, xtk_ref_local(item), page_post, page_bind_i(b, MSG_MENU, entries [i].choice)
			);
	}
}

static void menubar_menu(PageBuild *b, XtkNodeRef bar, char const *title, MenuEntry const *entries, int count) {
	XtkLocalId menu
	  = xtk_menufly_create(&b->txn, bar, &(XtkOverlayProps) {.set = XTK_OVERLAY_TEXT_MASK, .text = title});
	menu_entries(b, xtk_ref_local(menu), entries, count);
}

static void menubar_example(PageBuild *b, XtkNodeRef parent) {
	static MenuEntry const kFile [] = {
	  {"New",     NULL, 4},
	  {"Open...", NULL, 5},
	  {NULL,      NULL, 0},
	  {"Exit",    NULL, 6},
	};
	static MenuEntry const kEdit [] = {
	  {"Undo", "Undo", 7},
	  {"Redo", "Redo", 8},
	};
	static MenuEntry const kHelp [] = {
	  {"About", NULL, 9},
	};
	XtkNodeRef card = xtk_ref_local(ui_card(
	  &b->txn, page_example(b, parent, "A MenuBar with File / Edit / Help menus."), 0.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f
	));
	XtkNodeRef bar  = xtk_ref_local(xtk_menubar_create(&b->txn, card, NULL));
	menubar_menu(b, bar, "File", kFile, 4);
	menubar_menu(b, bar, "Edit", kEdit, 2);
	menubar_menu(b, bar, "Help", kHelp, 1);
}

static void dropdown_example(PageBuild *b, XtkNodeRef parent) {
	static MenuEntry const kActions [] = {
	  {"Send email",    "Mail",    1},
	  {"Open calendar", "Forward", 2},
	  {NULL,            NULL,      0},
	  {"Print",         "Print",   3},
	};
	PageDemo   demo = page_demo(b, page_example(b, parent, "A DropDownButton with a menu."));
	XtkLocalId drop
	  = xtk_dropdown_create(&b->txn, demo.items, &(XtkButtonProps) {.set = XTK_BUTTON_TEXT_MASK, .text = "Actions"});
	menu_entries(b, xtk_ref_local(xtk_menufly_create(&b->txn, xtk_ref_local(drop), NULL)), kActions, 4);
	page_keepout(b, MSG_MENU, page_output(b, demo.row, choice_text(b->app)));
}

bool page_menus_build(PageBuild *b) {
	page_sync(b, menus_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "Menus & links",
	  "MenuBar and DropDownButton open flyout menus; RepeatButton fires while held; HyperlinkButton opens a URL."
	);
	menubar_example(b, parent);
	dropdown_example(b, parent);
	{
		PageDemo   demo = page_demo(b, page_example(b, parent, "A RepeatButton keeps firing while pressed."));
		XtkLocalId rep  = xtk_repeat_create(
		  &b->txn, demo.items, &(XtkButtonProps) {.set = XTK_BUTTON_TEXT_MASK, .text = "Click and hold"}
		);
		xtk_repeat_oninvoke(&b->txn, xtk_ref_local(rep), page_post, page_bind(b, MSG_REPEAT));
		page_keepout(b, MSG_REPEAT, page_output(b, demo.row, repeat_text(b->app)));
	}
	{
		XtkNodeRef ex   = page_example(b, parent, "A HyperlinkButton opens the project page.");
		XtkNodeRef card = xtk_ref_local(ui_card(&b->txn, ex, 0.0f, XENT_FLEX_ALIGN_START, 16.0f));
		xtk_link_create(
		  &b->txn, card,
		  &(XtkLinkProps) {
			.set  = XTK_LINK_TEXT_MASK | XTK_LINK_URL_MASK,
			.text = "FluXent on GitHub",
			.url  = "https://github.com/Project-Xent",
		  }
		);
	}
	return true;
}
