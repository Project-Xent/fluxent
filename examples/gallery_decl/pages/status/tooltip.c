#include "gallery.h"

XtkEl *page_tooltip(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ToolTip", "Attach a tooltip to any element with xtk_tooltip; it shows after a short hover."),
		example(
		  ui, "A Button with a ToolTip.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				xtk_tooltip(ui, xtk_button(ui, "Hover me", (XtkButtonDesc) {0}), "A simple ToolTip on a Button"),
				xtk_tooltip(
				  ui, xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Save"}), "Icon-only buttons especially need one"
				)
			  )
			))
		  )
		)
	  )
	);
}
