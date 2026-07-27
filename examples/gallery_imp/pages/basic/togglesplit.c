#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_toggle_split(void *ud, bool checked) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .b = checked});
}

static void on_toggle_split_click(void *ud) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void on_menu_item(void *ctx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = b->i});
}

static void menu_add(GalleryApp *g, FluxMenu *menu, char const *label, int tag, int i) {
	flux_menu_additem(
	  menu, &(FluxMenuItemDef) {
			  .label        = label,
			  .enabled      = true,
			  .on_click     = on_menu_item,
			  .on_click_ctx = gallery_bind_i(g, tag, i),
			}
	);
}

static char const *const kToggleSplitMenu [] = {"(primary click)", "Align left", "Align center", "Align right"};

void                     page_toggle_split(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_toggle_split_sync);
	Model const *m    = &g->model;
	int          menu = m->toggle_split_menu >= 0 && m->toggle_split_menu <= 3 ? m->toggle_split_menu : 0;
	g_page_header(
	  g, parent, "ToggleSplitButton",
	  "The left zone toggles checked state; the chevron opens a menu of secondary actions."
	);
	{
		XentNodeId   ex     = g_example(g, parent, "Toggle + menu split button.");
		GalleryDemo  demo   = g_demo_card(g, ex);
		XentNodeId   row    = demo.items;
		GalleryBind *toggle = gallery_bind(g, MSG_TOGGLE_SPLIT);
		GalleryBind *click  = gallery_bind(g, MSG_TOGGLE_SPLIT_CLICK);
		XentNodeId   ts     = flux_togglesplit_init(&(FluxToggleSplitButtonDesc) {
		  .ctx           = gallery_ctx(g),
		  .store         = gallery_store(g),
		  .parent        = row,
		  .label         = "Bold",
		  .icon_name     = "Bold",
		  .checked       = m->toggle_split_checked,
		  .on_toggle     = on_toggle_split,
		  .on_toggle_ctx = toggle,
		  .on_click      = on_toggle_split_click,
		  .on_click_ctx  = click,
		});
		FluxMenu    *menu_f = flux_menu_create(flux_app_window(g->flux));
		if (menu_f) {
			flux_menu_setthememanager(menu_f, flux_app_theme(g->flux));
			flux_menu_settext(menu_f, flux_app_text(g->flux));
			menu_add(g, menu_f, "Align left", MSG_TOGGLE_SPLIT_MENU, 0);
			menu_add(g, menu_f, "Align center", MSG_TOGGLE_SPLIT_MENU, 1);
			menu_add(g, menu_f, "Align right", MSG_TOGGLE_SPLIT_MENU, 2);
			flux_prop_split_menu_ex(&(FluxContextFlyoutBindingInfo) {
			  .store  = gallery_store(g),
			  .id     = ts,
			  .menu   = menu_f,
			  .xctx   = gallery_ctx(g),
			  .window = flux_app_window(g->flux),
			});
		}
		g->live_output [MSG_TOGGLE_SPLIT] = g_output(
		  g, demo.row, gallery_fmt(g, "%s — %s", m->toggle_split_checked ? "On" : "Off", kToggleSplitMenu [menu])
		);
	}
}

void page_toggle_split_sync(GalleryApp *g) {
	int menu = g->model.toggle_split_menu;
	if (menu < 0 || menu > 3) menu = 0;
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_TOGGLE_SPLIT],
	  gallery_fmt(g, "%s — %s", g->model.toggle_split_checked ? "On" : "Off", kToggleSplitMenu [menu])
	);
}
