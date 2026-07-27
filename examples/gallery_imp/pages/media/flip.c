#include "app.h"
#include <math.h>
#include <stdio.h>

static XentNodeId flip_page(GalleryApp *g, XentNodeId parent, int i) {
	static char const *const kTitles [] = {"Sunrise", "Forest", "Ocean", "Dunes", "Aurora"};
	XentNodeId               card       = g_card(g, parent);
	xent_setitems(gallery_ctx(g), card, XENT_FLEX_ALIGN_CENTER);
	xent_setjustify(gallery_ctx(g), card, XENT_FLEX_JUSTIFY_CENTER);
	g_text(g, card, kTitles [i], 32.0f, FLUX_FONT_SEMI_BOLD);
	g_text(g, card, gallery_fmt(g, "Page %d of 5", i + 1), 13.0f, FLUX_FONT_REGULAR);
	return card;
}

static void on_flip(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

void page_flipview(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_flipview_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "FlipView",
	  "One page at a time: hover for the nav arrows, use the wheel or arrow keys, "
	  "or drive it from the PipsPager below — both post the same message."
	);
	{
		XentNodeId ex  = g_example(g, parent, "FlipView + PipsPager sharing one selected index.");
		XentNodeId col = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		xent_setitems(gallery_ctx(g), col, XENT_FLEX_ALIGN_CENTER);
		XentNodeId flip = flux_flip_init(&(FluxFlipViewDesc) {
		  .ctx       = gallery_ctx(g),
		  .store     = gallery_store(g),
		  .parent    = col,
		  .window    = flux_app_window(g->flux),
		  .on_select = on_flip,
		  .userdata  = gallery_bind(g, MSG_FLIP_SELECT),
		});
		xent_setsize(gallery_ctx(g), flip, (XentSize) {560.0f, 300.0f});
		XentNodeId host = flux_flip_content_node(gallery_store(g), flip);
		for (int i = 0; i < 5; i++) flip_page(g, host, i);
		if (m->flip_page > 0) flux_flip_select(gallery_store(g), flip, m->flip_page);
		XentNodeId pips = flux_pips_init(&(FluxPipsPagerDesc) {
		  .ctx       = gallery_ctx(g),
		  .store     = gallery_store(g),
		  .parent    = col,
		  .on_select = on_flip,
		  .userdata  = gallery_bind(g, MSG_FLIP_SELECT),
		});
		flux_pips_configure(gallery_store(g), pips, 5, m->flip_page, 0, XTK_PIPS_NAV_ON_HOVER);
		g->live_control [MSG_FLIP_SELECT] = flip;
		g->live_output [MSG_FLIP_SELECT]  = pips;
	}
}

void page_flipview_sync(GalleryApp *g) {
	flux_flip_select(gallery_store(g), g->live_control [MSG_FLIP_SELECT], g->model.flip_page);
	flux_pips_configure(
	  gallery_store(g), g->live_output [MSG_FLIP_SELECT], 5, g->model.flip_page, 0, XTK_PIPS_NAV_ON_HOVER
	);
}
