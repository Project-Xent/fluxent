#include "app.h"
#include <math.h>
#include <stdio.h>

static void on_tb_back(void *ctx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void on_tb_pane(void *ctx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

void page_title_bar(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_title_bar_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "TitleBar",
	  "A window title-bar band: an app icon, title and subtitle, plus optional back and pane-toggle "
	  "buttons. Opt into window integration to make the band drag the window."
	);
	{
		XentNodeId ex = g_example(g, parent, "Icon, title, subtitle, back and pane-toggle buttons.");
		flux_titlebar_init(&(FluxTitleBarDesc) {
		  .ctx              = gallery_ctx(g),
		  .store            = gallery_store(g),
		  .parent           = g_card(g, ex),
		  .window           = flux_app_window(g->flux),
		  .title            = "FluXent Gallery",
		  .subtitle         = "Preview",
		  .icon             = "Home",
		  .show_back        = true,
		  .show_pane_toggle = true,
		  .on_back          = on_tb_back,
		  .back_ctx         = gallery_bind(g, MSG_TB_BACK),
		  .on_pane_toggle   = on_tb_pane,
		  .pane_toggle_ctx  = gallery_bind(g, MSG_TB_PANE),
		});
	}
	{
		XentNodeId ex                = g_example(g, parent, "Button events.");
		g->live_output [MSG_TB_BACK] = g_text(
		  g, g_card(g, ex),
		  gallery_fmt(g, "Back clicks: %d.  Pane-toggle clicks: %d.", m->tb_back_clicks, m->tb_pane_clicks), 13.0f,
		  FLUX_FONT_REGULAR
		);
	}
}

void page_title_bar_sync(GalleryApp *g) {
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_TB_BACK],
	  gallery_fmt(g, "Back clicks: %d.  Pane-toggle clicks: %d.", g->model.tb_back_clicks, g->model.tb_pane_clicks)
	);
}
