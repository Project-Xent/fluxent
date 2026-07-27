#include "gallery.h"

static XtkEl *items_cell(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_card(
	  ui,
	  (XtkStackDesc) {
		.padding = {12, 12, 12, 12},
          .align = XENT_FLEX_ALIGN_CENTER
    },
	  xtk_kids(
		xtk_text(ui, xtk_fmt(ui, "Item %d", index), (XtkTextDesc) {.weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, "Tap to invoke", (XtkTextDesc) {.size = 11})
	  )
	);
}

XtkEl *page_items_view(XtkUi *ui, Model const *m) {
	char const *inv = m->items_invoke >= 0 ? xtk_fmt(ui, "Invoked: %d", m->items_invoke) : "Tap a cell to invoke.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ItemsView", "ItemsRepeater + layout + ItemContainer chrome. Here: a wrapping flow of 24 cards."
		),
		example(
		  ui, "Flow layout with ItemInvoked (no selection chrome).",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_itemsview(
				  ui,
				  (XtkItemsViewDesc) {
					.nitem                = 24,
					.itemh                = 96.0f,
					.itemw                = 140.0f,
					.gap                  = 8.0f,
					.layout               = XTK_ITEMS_LAYOUT_FLOW,
					.item                 = items_cell,
					.selection_mode       = XTK_LIST_SELECT_NONE,
					.item_invoked_enabled = true,
					.invoke               = xtk_msg(MSG_ITEMS_INVOKE),
				  }
				),
				NAN, 360.0f
			  ),
			  xtk_text(ui, inv, (XtkTextDesc) {.size = 13})
			)
		  )
		)
	  )
	);
}
