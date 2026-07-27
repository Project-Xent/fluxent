#include "gallery.h"

XtkEl *page_toggle(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ToggleSwitch", "Use a ToggleSwitch for binary on/off settings that take effect immediately."),
		example(
		  ui, "A simple ToggleSwitch.",
		  demo_card(
			ui,
			xtk_row(
			  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			  xtk_kids(
				xtk_switch(ui, (XtkToggleDesc) {.checked = m->light_on, .change = xtk_msg(MSG_LIGHT)}),
				xtk_text(ui, m->light_on ? "On" : "Off", (XtkTextDesc) {0})
			  )
			),
			m->light_on ? "The lights are on." : "The lights are off.", NULL
		  )
		)
	  )
	);
}
