#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "virtual.h"

static void on_refresh(void *ud, int dir) {
	( void ) dir;
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void fill_refresh_row(GalleryApp *g, XentNodeId item, int index) {
	g_text(g, item, gallery_fmt(g, "Row %d", index), 14.0f, FLUX_FONT_REGULAR);
}

void page_pull_refresh(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_pull_refresh_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "PullToRefresh", "A RefreshContainer wrapping scrollable content. Pull from the top edge to refresh."
	);
	{
		XentNodeId ex      = g_example(g, parent, "List inside a top-edge pull container.");
		XentNodeId col     = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		XentNodeId refresh = flux_refresh_init(&(FluxRefreshContainerDesc) {
		  .ctx        = gallery_ctx(g),
		  .store      = gallery_store(g),
		  .parent     = col,
		  .window     = flux_app_window(g->flux),
		  .on_refresh = on_refresh,
		  .userdata   = gallery_bind(g, MSG_REFRESH),
		});
		xent_seth(gallery_ctx(g), refresh, 360.0f);
		if (m->refresh_busy) flux_refresh_set_refreshing(gallery_store(g), refresh, true);
		XentNodeId inner = flux_refresh_content_node(gallery_store(g), refresh);
		gallery_list_create(
		  g, inner, XTK_LIST_KIND_LIST, 40, 40.0f, 0.0f, 1, -1, XTK_LIST_SELECT_NONE, MSG_NONE, fill_refresh_row, 360.0f
		);
		g->live_control [MSG_REFRESH] = refresh;
		g->live_output [MSG_REFRESH]  = g_text(
		  g, col, gallery_fmt(g, "Refresh count: %d. %s", m->refresh_count, m->refresh_busy ? "Refreshing…" : "Idle"),
		  13.0f, FLUX_FONT_REGULAR
		);
		XentNodeId done = g_button(g, col, "End refresh", NULL, 0, m->refresh_busy);
		g_on_invoke(g, done, MSG_REFRESH_DONE);
		g->live_control [MSG_REFRESH_DONE] = done;
	}
}

void page_pull_refresh_sync(GalleryApp *g) {
	flux_refresh_set_refreshing(gallery_store(g), g->live_control [MSG_REFRESH], g->model.refresh_busy);
	flux_prop_button_enabled(gallery_store(g), g->live_control [MSG_REFRESH_DONE], g->model.refresh_busy);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_REFRESH],
	  gallery_fmt(g, "Refresh count: %d. %s", g->model.refresh_count, g->model.refresh_busy ? "Refreshing…" : "Idle")
	);
}
