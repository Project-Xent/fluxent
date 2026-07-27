#include "gallery.h"

XtkEl *page_header(XtkUi *ui, char const *title, char const *blurb) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8},
	  xtk_kids(
		xtk_text(ui, title, (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, blurb, (XtkTextDesc) {.size = 13})
	  )
	);
}

XtkEl *example(XtkUi *ui, char const *caption, XtkEl *content) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(xtk_text(ui, caption, (XtkTextDesc) {.size = 14}), content)
	);
}

XtkEl *output_col(XtkUi *ui, char const *value) {
	return xtk_sized(
	  xtk_column(
		ui, (XtkStackDesc) {.gap = 4},
		xtk_kids(
		  xtk_text(ui, "Output:", (XtkTextDesc) {.size = 12, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, value, (XtkTextDesc) {.size = 14})
		)
	  ),
	  260, NAN
	);
}

XtkEl *demo_card(XtkUi *ui, XtkEl *control, char const *output, XtkEl *extra) {
	XtkEl *items = xtk_row(
	  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP}, xtk_kids(control, extra)
	);
	return xtk_card(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
	  xtk_kids(xtk_row(
		ui,
		(XtkStackDesc) {
		  .gap     = 16,
		  .align   = XENT_FLEX_ALIGN_CENTER,
		  .justify = XENT_FLEX_JUSTIFY_SPACE_BETWEEN,
		  .fill    = true,
		  .wrap    = XENT_FLEX_WRAP},
		xtk_kids(items, output ? output_col(ui, output) : NULL)
	  ))
	);
}

XtkEl *page_category(XtkUi *ui, char const *title, char const *blurb) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, title, blurb),
		xtk_text(ui, "Expand this group in the navigation pane to pick a control.", (XtkTextDesc) {.size = 13})
	  )
	);
}
