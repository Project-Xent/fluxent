#include "gallery.h"
#include <string.h>

static XtkEl *flip_page_el(XtkUi *ui, int i) {
	static char const *const kTitles [] = {"Sunrise", "Forest", "Ocean", "Dunes", "Aurora"};
	return xtk_card(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER, .justify = XENT_FLEX_JUSTIFY_CENTER},
	  xtk_kids(
		xtk_text(ui, kTitles [i], (XtkTextDesc) {.size = 32, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, xtk_fmt(ui, "Page %d of 5", i + 1), (XtkTextDesc) {.size = 13})
	  )
	);
}

XtkEl *page_flipview(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "FlipView",
		  "One page at a time: hover for the nav arrows, use the wheel or arrow keys, "
		  "or drive it from the PipsPager below — both post the same message."
		),
		example(
		  ui, "FlipView + PipsPager sharing one selected index.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			xtk_kids(
			  xtk_sized(
				xtk_flip(
				  ui, (XtkFlipDesc) {.sel = m->flip_page, .select = xtk_msg(MSG_FLIP_SELECT)},
				  xtk_kids(
					flip_page_el(ui, 0), flip_page_el(ui, 1), flip_page_el(ui, 2), flip_page_el(ui, 3),
					flip_page_el(ui, 4)
				  )
				),
				560.0f, 300.0f
			  ),
			  xtk_pips(
				ui, (XtkPipsDesc) {
					  .nitem  = 5,
					  .sel    = m->flip_page,
					  .nav    = XTK_PIPS_NAV_ON_HOVER,
					  .select = xtk_msg(MSG_FLIP_SELECT),
					}
			  )
			)
		  )
		)
	  )
	);
}
