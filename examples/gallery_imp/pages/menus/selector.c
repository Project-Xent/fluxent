#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_select_i(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static XtkSelectorItem const kSelectors [] = {
  {.text = "All",    .icon = "List"     },
  {.text = "Music",  .icon = "MusicNote"},
  {.text = "Video",  .icon = "Video"    },
  {.text = "Photos", .icon = "Pictures" },
};

void page_selector_bar(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_selector_bar_sync);
	Model const *m = &g->model;
	char         status [128];
	if (m->selector_sel >= 0 && m->selector_sel < 4)
		snprintf(status, sizeof(status), "Filter: %s", kSelectors [m->selector_sel].text);
	else snprintf(status, sizeof(status), "No filter selected.");
	g_page_header(g, parent, "SelectorBar", "A horizontal strip of pill toggles — one selected item at a time.");
	{
		XentNodeId  ex   = g_example(g, parent, "Icon+text selector items.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		XentNodeId  bar  = flux_selector_init(&(FluxSelectorBarDesc) {
		  .ctx        = gallery_ctx(g),
		  .store      = gallery_store(g),
		  .parent     = row,
		  .input      = flux_app_input(g->flux),
		  .window     = flux_app_window(g->flux),
		  .item_count = 4,
		  .selected   = m->selector_sel,
		  .on_select  = on_select_i,
		  .userdata   = gallery_bind(g, MSG_SELECTOR_BAR),
		});
		for (int i = 0; i < 4; i++) {
			wchar_t const *glyph = kSelectors [i].icon ? flux_icon_lookup(kSelectors [i].icon) : NULL;
			flux_selector_set_item(
			  gallery_store(g), bar, i, kSelectors [i].text, glyph && glyph [0] ? ( uint32_t ) glyph [0] : 0, false
			);
		}
		g->live_output [MSG_SELECTOR_BAR] = g_output(g, demo.row, status);
	}
}

void page_selector_bar_sync(GalleryApp *g) {
	int         selected = g->model.selector_sel;
	char const *status   = selected >= 0 && selected < 4 ? gallery_fmt(g, "Filter: %s", kSelectors [selected].text)
	                                                     : "No filter selected.";
	xent_settext(gallery_ctx(g), g->live_output [MSG_SELECTOR_BAR], status);
}
