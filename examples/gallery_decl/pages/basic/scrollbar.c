#include "gallery.h"

XtkEl *page_scroll_bar(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ScrollBar",
		  "A ScrollBar reports where the user asked to scroll to; it moves nothing on its own. "
		  "ViewportSize is in the same units as the range and decides how much of the track the thumb covers."
		),
		example(
		  ui, "A vertical ScrollBar over a range of 1000 with a viewport of 200.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_scroll_bar(
				ui,
				(XtkScrollBarDesc) {
				  .max = 1000, .value = m->scrollbar_v, .viewport = 200, .change = xtk_msg(MSG_SCROLLBAR_V)}
			  ),
			  NAN, 200
			),
			xtk_fmt(ui, "%.0f", m->scrollbar_v), NULL
		  )
		),
		example(
		  ui, "A horizontal ScrollBar. Page steps come from LargeChange, the line buttons from SmallChange.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_scroll_bar(
				ui,
				(XtkScrollBarDesc) {
				  .horizontal   = true,
				  .max          = 500,
				  .value        = m->scrollbar_h,
				  .viewport     = 100,
				  .small_change = 5,
				  .large_change = 100,
				  .change       = xtk_msg(MSG_SCROLLBAR_H)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f", m->scrollbar_h), NULL
		  )
		),
		example(
		  ui, "A disabled ScrollBar.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_scroll_bar(ui, (XtkScrollBarDesc) {.max = 1000, .value = 300, .viewport = 200, .disabled = true}),
			  NAN, 200
			),
			NULL, NULL
		  )
		)
	  )
	);
}
