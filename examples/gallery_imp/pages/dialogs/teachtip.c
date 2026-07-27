#include "app.h"
#include <math.h>
#include <stdio.h>
#include "host/flux_prop_apply.h"
#include "render/flux_icon.h"

static void on_tip_action(void *ud) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag});
}

static void on_tip_close(void *ud, FluxTeachingTipCloseReason reason) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = ( int ) reason});
}

void page_teaching_tip(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_teaching_tip_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "TeachingTip", "A lightweight teaching surface with subtitle, action, and close affordances."
	);
	{
		XentNodeId  ex   = g_example(g, parent, "Untargeted tip toggled from the model.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		XentNodeId  btn  = g_button(g, row, m->tip_open ? "Hide tip" : "Show tip", NULL, 0, true);
		g_on_invoke(g, btn, MSG_TIP_OPEN);
		g->live_control [MSG_TIP_OPEN] = btn;
		g->live_output [MSG_TIP_OPEN]  = g_output(g, demo.row, m->tip_open ? "Tip is open." : "Tip is closed.");
	}
	{
		wchar_t const *glyph = flux_icon_lookup("Lightbulb");
		XentNodeId     tip   = flux_teachtip_init(&(FluxTeachingTipDesc) {
		  .ctx           = gallery_ctx(g),
		  .store         = gallery_store(g),
		  .parent        = parent,
		  .window        = flux_app_window(g->flux),
		  .text          = flux_app_text(g->flux),
		  .theme         = flux_app_theme(g->flux),
		  .input         = flux_app_input(g->flux),
		  .title         = "Welcome to FluXent",
		  .subtitle      = "Explore the WinUI 3 control gallery.",
		  .icon_glyph    = glyph && glyph [0] ? ( uint32_t ) glyph [0] : 0,
		  .action_text   = "Got it",
		  .action_accent = true,
		  .untargeted    = true,
		  .on_action     = on_tip_action,
		  .action_ctx    = gallery_bind(g, MSG_TIP_ACTION),
		  .on_close      = on_tip_close,
		  .close_ctx     = gallery_bind(g, MSG_TIP_CLOSE),
		});
		if (m->tip_open) flux_teachtip_show(gallery_store(g), tip);
		else flux_teachtip_hide(gallery_store(g), tip);
		g->live_control [MSG_TIP_CLOSE] = tip;
	}
	{
		XentNodeId ex                   = g_example(g, parent, "Tip events.");
		g->live_output [MSG_TIP_ACTION] = g_text(
		  g, g_card(g, ex),
		  gallery_fmt(g, "Action clicks: %d. Last close reason: %d.", m->tip_actions, m->tip_close_reason), 13.0f,
		  FLUX_FONT_REGULAR
		);
	}
}

void page_teaching_tip_sync(GalleryApp *g) {
	FluxStore *store = gallery_store(g);
	if (g->model.tip_open) flux_teachtip_show(store, g->live_control [MSG_TIP_CLOSE]);
	else flux_teachtip_hide(store, g->live_control [MSG_TIP_CLOSE]);
	flux_prop_button_label(store, g->live_control [MSG_TIP_OPEN], g->model.tip_open ? "Hide tip" : "Show tip");
	xent_settext(gallery_ctx(g), g->live_output [MSG_TIP_OPEN], g->model.tip_open ? "Tip is open." : "Tip is closed.");
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_TIP_ACTION],
	  gallery_fmt(g, "Action clicks: %d. Last close reason: %d.", g->model.tip_actions, g->model.tip_close_reason)
	);
}
