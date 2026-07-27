#include "gallery.h"

XtkEl *page_radio(XtkUi *ui, Model const *m) {
	static char const *const kOptions [] = {"Option 1", "Option 2", "Option 3"};
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "RadioButton", "RadioButtons present a set of mutually exclusive options — the model holds the index."
		),
		example(
		  ui, "A group of RadioButtons.",
		  demo_card(
			ui,
			xtk_column(
			  ui, (XtkStackDesc) {.gap = 8},
			  xtk_kids(
				xtk_radio(ui, kOptions [0], (XtkToggleDesc) {.checked = m->radio == 0, .change = xtk_msg(MSG_RADIO_0)}),
				xtk_radio(ui, kOptions [1], (XtkToggleDesc) {.checked = m->radio == 1, .change = xtk_msg(MSG_RADIO_1)}),
				xtk_radio(ui, kOptions [2], (XtkToggleDesc) {.checked = m->radio == 2, .change = xtk_msg(MSG_RADIO_2)})
			  )
			),
			xtk_fmt(ui, "You selected %s", kOptions [m->radio]), NULL
		  )
		)
	  )
	);
}
