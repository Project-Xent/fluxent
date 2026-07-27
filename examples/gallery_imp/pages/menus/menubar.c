#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_menu_item(void *ctx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = b->i});
}

static void on_invoke_ud(void *ud) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void menu_add(GalleryApp *g, FluxMenu *menu, char const *label, char const *icon, int tag, int i) {
	char glyph [8] = {0};
	if (icon) flux_icon_to_utf8(flux_icon_lookup(icon), glyph, sizeof(glyph));
	flux_menu_additem(
	  menu, &(FluxMenuItemDef) {
			  .label        = label,
			  .icon_glyph   = glyph [0] ? glyph : NULL,
			  .enabled      = true,
			  .on_click     = on_menu_item,
			  .on_click_ctx = gallery_bind_i(g, tag, i),
			}
	);
}

static void fill_dropdown(GalleryApp *g, FluxMenu *menu) {
	menu_add(g, menu, "Send email", "Mail", MSG_MENU, 1);
	menu_add(g, menu, "Open calendar", "Forward", MSG_MENU, 2);
	flux_menu_addsep(menu);
	menu_add(g, menu, "Print", "Print", MSG_MENU, 3);
}

static void bind_menu_flyout(GalleryApp *g, XentNodeId node, FluxMenu *menu) {
	flux_menu_setthememanager(menu, flux_app_theme(g->flux));
	flux_menu_settext(menu, flux_app_text(g->flux));
	flux_prop_menu_ex(&(FluxContextFlyoutBindingInfo) {
	  .store  = gallery_store(g),
	  .id     = node,
	  .menu   = menu,
	  .xctx   = gallery_ctx(g),
	  .window = flux_app_window(g->flux),
	});
}

static char const *const kMenuChoices [] = {
  "(nothing yet)", "Send email", "Open calendar", "Print", "File > New", "File > Open...", "File > Exit", "Edit > Undo",
  "Edit > Redo", "Help > About"};

void page_menus(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_menus_sync);
	Model const *m      = &g->model;
	int          choice = (m->menu_choice >= 0 && m->menu_choice <= 9) ? m->menu_choice : 0;
	g_page_header(
	  g, parent, "Menus & links",
	  "MenuBar and DropDownButton open flyout menus; RepeatButton fires while held; "
	  "HyperlinkButton opens a URL."
	);
	{
		XentNodeId ex   = g_example(g, parent, "A MenuBar with File / Edit / Help menus.");
		XentNodeId bar  = flux_menubar_init(&(FluxMenuBarDesc) {
		  .ctx    = gallery_ctx(g),
		  .store  = gallery_store(g),
		  .parent = g_card(g, ex),
		  .window = flux_app_window(g->flux),
		  .text   = flux_app_text(g->flux),
		  .theme  = flux_app_theme(g->flux),
		});
		FluxMenu  *file = flux_menubar_add_menu(gallery_store(g), bar, "File");
		if (file) {
			menu_add(g, file, "New", NULL, MSG_MENU, 4);
			menu_add(g, file, "Open...", NULL, MSG_MENU, 5);
			flux_menu_addsep(file);
			menu_add(g, file, "Exit", NULL, MSG_MENU, 6);
		}
		FluxMenu *edit = flux_menubar_add_menu(gallery_store(g), bar, "Edit");
		if (edit) {
			menu_add(g, edit, "Undo", "Undo", MSG_MENU, 7);
			menu_add(g, edit, "Redo", "Redo", MSG_MENU, 8);
		}
		FluxMenu *help = flux_menubar_add_menu(gallery_store(g), bar, "Help");
		if (help) menu_add(g, help, "About", NULL, MSG_MENU, 9);
	}
	{
		XentNodeId  ex   = g_example(g, parent, "A DropDownButton with a menu.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		XentNodeId  dd   = flux_dropdown_init(&(FluxDropDownButtonDesc) {
		  .ctx    = gallery_ctx(g),
		  .store  = gallery_store(g),
		  .parent = row,
		  .label  = "Actions",
		});
		FluxMenu   *menu = flux_menu_create(flux_app_window(g->flux));
		if (menu) {
			fill_dropdown(g, menu);
			bind_menu_flyout(g, dd, menu);
		}
		g->live_output [MSG_MENU] = g_output(g, demo.row, gallery_fmt(g, "Last choice: %s", kMenuChoices [choice]));
	}
	{
		XentNodeId  ex   = g_example(g, parent, "A RepeatButton keeps firing while pressed.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		XentNodeId  rb   = flux_repeat_init(&(FluxButtonDesc) {
		  .ctx      = gallery_ctx(g),
		  .store    = gallery_store(g),
		  .parent   = row,
		  .label    = "Click and hold",
		  .on_click = on_invoke_ud,
		  .userdata = gallery_bind(g, MSG_REPEAT),
		});
		( void ) rb;
		g->live_output [MSG_REPEAT]
		  = g_output(g, demo.row, gallery_fmt(g, "Fired %d time%s", m->repeats, m->repeats == 1 ? "" : "s"));
	}
	{
		XentNodeId ex = g_example(g, parent, "A HyperlinkButton opens the project page.");
		flux_link_init(&(FluxHyperlinkDesc) {
		  .ctx    = gallery_ctx(g),
		  .store  = gallery_store(g),
		  .parent = g_card(g, ex),
		  .label  = "FluXent on GitHub",
		  .url    = "https://github.com/Project-Xent",
		});
	}
}

void page_menus_sync(GalleryApp *g) {
	int choice = g->model.menu_choice;
	if (choice < 0 || choice > 9) choice = 0;
	xent_settext(gallery_ctx(g), g->live_output [MSG_MENU], gallery_fmt(g, "Last choice: %s", kMenuChoices [choice]));
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_REPEAT],
	  gallery_fmt(g, "Fired %d time%s", g->model.repeats, g->model.repeats == 1 ? "" : "s")
	);
}
