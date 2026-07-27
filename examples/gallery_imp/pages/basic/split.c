#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static char const *const kSplitActions [] = {"(nothing yet)", "Saved", "Saved as...", "Saved all"};

static void              on_split_primary(void *ud) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void on_split_menu(void *ctx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = b->i});
}

static void menu_add(GalleryApp *g, FluxMenu *menu, char const *label, char const *icon, int tag, int i) {
	char glyph [8] = {0};
	if (icon) flux_icon_to_utf8(flux_icon_lookup(icon), glyph, sizeof(glyph));
	flux_menu_additem(
	  menu, &(FluxMenuItemDef) {
			  .label        = label,
			  .icon_glyph   = glyph [0] ? glyph : NULL,
			  .enabled      = true,
			  .on_click     = on_split_menu,
			  .on_click_ctx = gallery_bind_i(g, tag, i),
			}
	);
}

static void bind_split_menu(GalleryApp *g, XentNodeId split, FluxMenu *menu) {
	flux_menu_setthememanager(menu, flux_app_theme(g->flux));
	flux_menu_settext(menu, flux_app_text(g->flux));
	menu_add(g, menu, "Save as...", NULL, MSG_SPLIT_MENU, 0);
	menu_add(g, menu, "Save all", NULL, MSG_SPLIT_MENU, 1);
	flux_prop_split_menu_ex(&(FluxContextFlyoutBindingInfo) {
	  .store  = gallery_store(g),
	  .id     = split,
	  .menu   = menu,
	  .xctx   = gallery_ctx(g),
	  .window = flux_app_window(g->flux),
	});
}

void page_split(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_split_sync);
	Model const *m      = &g->model;
	int          action = (m->split_action >= 0 && m->split_action <= 3) ? m->split_action : 0;
	g_page_header(
	  g, parent, "SplitButton", "The left zone fires the default action; the chevron opens a menu of alternatives."
	);
	{
		XentNodeId  ex    = g_example(g, parent, "A SplitButton for saving with alternatives.");
		GalleryDemo demo  = g_demo_card(g, ex);
		XentNodeId  row   = demo.items;
		XentNodeId  split = flux_split_init(&(FluxSplitButtonDesc) {
		  .ctx       = gallery_ctx(g),
		  .store     = gallery_store(g),
		  .parent    = row,
		  .label     = "Save",
		  .icon_name = "Save",
		  .on_click  = on_split_primary,
		  .userdata  = gallery_bind(g, MSG_SPLIT_PRIMARY),
		});
		FluxMenu   *menu  = flux_menu_create(flux_app_window(g->flux));
		if (menu) bind_split_menu(g, split, menu);
		g->live_output [MSG_SPLIT_PRIMARY]
		  = g_output(g, demo.row, gallery_fmt(g, "Last action: %s", kSplitActions [action]));
	}
}

void page_split_sync(GalleryApp *g) {
	int action = g->model.split_action;
	if (action < 0 || action > 3) action = 0;
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_SPLIT_PRIMARY], gallery_fmt(g, "Last action: %s", kSplitActions [action])
	);
}
