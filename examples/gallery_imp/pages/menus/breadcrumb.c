#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_select_i(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

void page_breadcrumb(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_breadcrumb_sync);
	static char const *const kCrumbs [] = {"Home", "Documents", "Projects", "FluXent"};
	Model const             *m          = &g->model;
	char                     status [128];
	if (m->breadcrumb_click >= 0 && m->breadcrumb_click < 4)
		snprintf(status, sizeof(status), "Clicked: %s", kCrumbs [m->breadcrumb_click]);
	else snprintf(status, sizeof(status), "Click a crumb to navigate.");
	g_page_header(g, parent, "BreadcrumbBar", "Shows the path to the current location. Every crumb posts ItemClicked.");
	{
		XentNodeId  ex   = g_example(g, parent, "A four-segment breadcrumb.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		flux_breadcrumb_init(&(FluxBreadcrumbBarDesc) {
		  .ctx             = gallery_ctx(g),
		  .store           = gallery_store(g),
		  .parent          = row,
		  .window          = flux_app_window(g->flux),
		  .text            = flux_app_text(g->flux),
		  .theme           = flux_app_theme(g->flux),
		  .input           = flux_app_input(g->flux),
		  .items           = kCrumbs,
		  .item_count      = 4,
		  .on_item_clicked = on_select_i,
		  .userdata        = gallery_bind(g, MSG_BREADCRUMB),
		});
		g->live_output [MSG_BREADCRUMB] = g_output(g, demo.row, status);
	}
}

void page_breadcrumb_sync(GalleryApp *g) {
	static char const *const crumbs [] = {"Home", "Documents", "Projects", "FluXent"};
	int                      clicked   = g->model.breadcrumb_click;
	char const              *status
	  = clicked >= 0 && clicked < 4 ? gallery_fmt(g, "Clicked: %s", crumbs [clicked]) : "Click a crumb to navigate.";
	xent_settext(gallery_ctx(g), g->live_output [MSG_BREADCRUMB], status);
}
