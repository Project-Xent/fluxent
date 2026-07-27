#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_expand_ud(void *ud, bool expanded) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .b = expanded});
}

void page_expander(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_expander_sync);
	Model const *m = &g->model;
	g_page_header(g, parent, "Expander", "An Expander shows a header and reveals a content region when toggled.");
	{
		XentNodeId content = XENT_NODE_INVALID;
		XentNodeId exp     = flux_expander_init(&(FluxExpanderDesc) {
		  .ctx         = gallery_ctx(g),
		  .store       = gallery_store(g),
		  .parent      = g_example(g, parent, "An Expander with interactive content."),
		  .window      = flux_app_window(g->flux),
		  .header      = "This Expander is bound to the model",
		  .expanded    = m->expanded,
		  .on_toggle   = on_expand_ud,
		  .userdata    = gallery_bind(g, MSG_EXPAND),
		  .out_content = &content,
		});
		( void ) exp;
		if (content != XENT_NODE_INVALID) {
			g_text(g, content, "Anything can live in here — it is a normal container.", 14.0f, FLUX_FONT_REGULAR);
			XentNodeId cb = g_checkbox(g, content, "Even this checkbox from the CheckBox page", m->check_a, true);
			g_on_change_bool(g, cb, MSG_CHECK_A);
		}
	}
}

void page_expander_sync(GalleryApp *g) { ( void ) g; }
