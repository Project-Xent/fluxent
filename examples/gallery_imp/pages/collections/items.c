#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "virtual.h"

static void fill_items_cell(GalleryApp *g, XentNodeId item, int index) {
	XentNodeId card = g_card(g, item);
	xent_setp(gallery_ctx(g), card, (XentInsets) {12, 12, 12, 12});
	xent_setitems(gallery_ctx(g), card, XENT_FLEX_ALIGN_CENTER);
	g_text(g, card, gallery_fmt(g, "Item %d", index), 14.0f, FLUX_FONT_SEMI_BOLD);
	g_text(g, card, "Tap to invoke", 11.0f, FLUX_FONT_REGULAR);
}

void page_items_view(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_items_view_sync);
	Model const *m   = &g->model;
	char const  *inv = m->items_invoke >= 0 ? gallery_fmt(g, "Invoked: %d", m->items_invoke) : "Tap a cell to invoke.";
	g_page_header(
	  g, parent, "ItemsView", "ItemsRepeater + layout + ItemContainer chrome. Here: a wrapping flow of 24 cards."
	);
	{
		XentNodeId ex   = g_example(g, parent, "Flow layout with ItemInvoked (no selection chrome).");
		XentNodeId col  = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		XentNodeId view = flux_itemsview_init(&(FluxItemsViewDesc) {
		  .ctx                  = gallery_ctx(g),
		  .store                = gallery_store(g),
		  .parent               = col,
		  .layout               = XTK_ITEMS_LAYOUT_FLOW,
		  .selection_mode       = XTK_LIST_SELECT_NONE,
		  .item_spacing         = 8.0f,
		  .item_invoked_enabled = true,
		  .input                = flux_app_input(g->flux),
		  .on_invoke            = gallery_list_invoke,
		  .userdata             = gallery_bind(g, MSG_ITEMS_INVOKE),
		});
		xent_seth(gallery_ctx(g), view, 360.0f);
		flux_itemsview_set_extent(gallery_store(g), view, 24, 96.0f, 140.0f, 4);
		XentNodeId host = flux_itemsview_content_node(gallery_store(g), view);
		gallery_list_clear(g, host);
		for (int i = 0; i < 24; i++) {
			XentNodeId item = flux_listitem_init(&(FluxListItemDesc) {
			  .ctx = gallery_ctx(g), .store = gallery_store(g), .parent = host});
			float      x    = ( float ) (i % 4) * 148.0f;
			float      y    = ( float ) (i / 4) * 104.0f;
			fluxlistitem_set_place(gallery_store(g), item, i, x, y);
			fill_items_cell(g, item, i);
		}
		flux_list_set_realized(gallery_store(g), view, 0, 23, 4);
		g->live_control [MSG_ITEMS_INVOKE] = view;
		g->live_output [MSG_ITEMS_INVOKE]  = g_text(g, col, inv, 13.0f, FLUX_FONT_REGULAR);
	}
}

void page_items_view_sync(GalleryApp *g) {
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_ITEMS_INVOKE],
	  g->model.items_invoke >= 0 ? gallery_fmt(g, "Invoked: %d", g->model.items_invoke) : "Tap a cell to invoke."
	);
}
