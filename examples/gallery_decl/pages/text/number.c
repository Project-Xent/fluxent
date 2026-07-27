#include "gallery.h"

XtkEl *page_numberbox(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "NumberBox", "Use a NumberBox for numeric input with spin buttons, wheel, and keyboard support."
		),
		example(
		  ui, "A NumberBox with spin buttons (0–100).",
		  demo_card(
			ui,
			xtk_number(
			  ui, (XtkNumberDesc) {.max = 100, .step = 1, .value = m->quantity, .change = xtk_msg(MSG_QUANTITY)}
			),
			xtk_fmt(ui, "Quantity: %.0f", m->quantity), NULL
		  )
		)
	  )
	);
}
