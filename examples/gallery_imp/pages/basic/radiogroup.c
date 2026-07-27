#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_select_i(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static FluxRadioItem const kRbItems [] = {
  {.label = "Option A"},
  {.label = "Option B"},
  {.label = "Option C"},
};

void page_radio_buttons(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_radio_buttons_sync);
	Model const *m      = &g->model;
	char const  *status = m->radio_buttons_sel >= 0 && m->radio_buttons_sel < 3 ? kRbItems [m->radio_buttons_sel].label
	                                                                            : "Nothing selected";
	g_page_header(
	  g, parent, "RadioButtons", "A grouped set of mutually exclusive radio options — one control, one selection index."
	);
	{
		XentNodeId  ex   = g_example(g, parent, "A RadioButtons group with a header.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		flux_radiogroup_init(&(FluxRadioButtonsDesc) {
		  .ctx        = gallery_ctx(g),
		  .store      = gallery_store(g),
		  .parent     = row,
		  .input      = flux_app_input(g->flux),
		  .header     = "Background",
		  .items      = kRbItems,
		  .item_count = 3,
		  .selected   = m->radio_buttons_sel,
		  .on_select  = on_select_i,
		  .userdata   = gallery_bind(g, MSG_RADIO_BUTTONS),
		});
		g->live_output [MSG_RADIO_BUTTONS] = g_output(g, demo.row, gallery_fmt(g, "Selected: %s", status));
	}
}

void page_radio_buttons_sync(GalleryApp *g) {
	int         selected = g->model.radio_buttons_sel;
	char const *status   = selected >= 0 && selected < 3 ? kRbItems [selected].label : "Nothing selected";
	xent_settext(gallery_ctx(g), g->live_output [MSG_RADIO_BUTTONS], gallery_fmt(g, "Selected: %s", status));
}
