#include "app.h"
#include <math.h>
#include <stdio.h>

static void on_pager(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

void page_pager(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_pager_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "PagerControl",
	  "A numeric/arrow pager (distinct from the dot PipsPager): First/Previous/Next/Last buttons around a "
	  "number panel with ellipses, or a compact page-of-total label."
	);
	{
		XentNodeId ex                      = g_example(g, parent, "Number panel (32 pages) with First/Last buttons.");
		XentNodeId card                    = g_col(g, g_card(g, ex), 12.0f, 0, 0, 0, 0);
		XentNodeId pager                   = flux_pager_init(&(FluxPagerDesc) {
		  .ctx          = gallery_ctx(g),
		  .store        = gallery_store(g),
		  .parent       = card,
		  .count        = 32,
		  .selected     = m->pager_page,
		  .first_button = XTK_PAGER_BUTTON_AUTO,
		  .last_button  = XTK_PAGER_BUTTON_AUTO,
		  .on_select    = on_pager,
		  .userdata     = gallery_bind(g, MSG_PAGER_SELECT),
		});
		g->live_control [MSG_PAGER_SELECT] = pager;
		g->live_output [MSG_PAGER_SELECT]
		  = g_text(g, card, gallery_fmt(g, "Page %d of 32", m->pager_page + 1), 13.0f, FLUX_FONT_REGULAR);
	}
	{
		XentNodeId ex                     = g_example(g, parent, "Number-text display.");
		g->live_control [MSG_FLIP_SELECT] = flux_pager_init(&(FluxPagerDesc) {
		  .ctx          = gallery_ctx(g),
		  .store        = gallery_store(g),
		  .parent       = g_card(g, ex),
		  .count        = 32,
		  .selected     = m->pager_page,
		  .display_mode = XTK_PAGER_DISPLAY_NUMBER_TEXT,
		  .prefix       = "Page",
		  .suffix       = "of",
		  .on_select    = on_pager,
		  .userdata     = gallery_bind(g, MSG_PAGER_SELECT),
		});
	}
}

void page_pager_sync(GalleryApp *g) {
	flux_pager_set_selected(gallery_store(g), g->live_control [MSG_PAGER_SELECT], g->model.pager_page);
	flux_pager_set_selected(gallery_store(g), g->live_control [MSG_FLIP_SELECT], g->model.pager_page);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_PAGER_SELECT], gallery_fmt(g, "Page %d of 32", g->model.pager_page + 1)
	);
}
