#include "app.h"
#include <math.h>
#include <stdio.h>
#include "host/flux_prop_apply.h"

static void on_bar_close(void *ud) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = b->i});
}

void page_infobar(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_infobar_sync);
	static char const *const kTitles [BAR_COUNT] = {"Informational", "Success", "Warning", "Error"};
	Model const             *m                   = &g->model;
	g_page_header(
	  g, parent, "InfoBar", "Use an InfoBar for essential status messages. Closing one just flips a model flag."
	);
	{
		XentNodeId ex  = g_example(g, parent, "One bar per severity; close them, then restore.");
		XentNodeId col = g_col(g, ex, 8.0f, 0, 0, 0, 0);
		for (int i = 0; i < BAR_COUNT; i++) {
			XentNodeId bar                      = flux_infobar_init(&(FluxInfoBarDesc) {
			  .ctx         = gallery_ctx(g),
			  .store       = gallery_store(g),
			  .parent      = col,
			  .severity    = ( XtkInfoBarSeverity ) i,
			  .title       = kTitles [i],
			  .message     = "Essential app message; closable like the real control.",
			  .is_closable = true,
			  .on_close    = on_bar_close,
			  .userdata    = gallery_bind_i(g, MSG_BAR_CLOSE, i),
			});
			g->live_control [MSG_BAR_CLOSE + i] = bar;
			flux_prop_infobar_open(gallery_store(g), bar, m->bar_open [i]);
		}
		XentNodeId btn = g_button(g, col, "Restore all info bars", NULL, 0, true);
		g_on_invoke(g, btn, MSG_BARS_RESET);
	}
}

void page_infobar_sync(GalleryApp *g) {
	for (int i = 0; i < BAR_COUNT; i++)
		flux_prop_infobar_open(gallery_store(g), g->live_control [MSG_BAR_CLOSE + i], g->model.bar_open [i]);
}
