#include "gallery.h"

static XtkRadioItem const kRbItems [] = {
  {.label = "Option A"},
  {.label = "Option B"},
  {.label = "Option C"},
};

XtkEl *page_radio_buttons(XtkUi *ui, Model const *m) {
	char const *status = m->radio_buttons_sel >= 0 && m->radio_buttons_sel < 3 ? kRbItems [m->radio_buttons_sel].label
	                                                                           : "Nothing selected";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "RadioButtons", "A grouped set of mutually exclusive radio options — one control, one selection index."
		),
		example(
		  ui, "A RadioButtons group with a header.",
		  demo_card(
			ui,
			xtk_radiogroup(
			  ui,
			  (XtkRadioButtonsDesc) {
				.header = "Background",
				.items  = kRbItems,
				.nitem  = 3,
				.sel    = m->radio_buttons_sel,
				.select = xtk_msg(MSG_RADIO_BUTTONS),
			  }
			),
			xtk_fmt(ui, "Selected: %s", status), NULL
		  )
		)
	  )
	);
}
