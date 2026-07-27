#include "gallery.h"
#include <stdio.h>

XtkEl *page_expander(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Expander", "An Expander shows a header and reveals a content region when toggled."),
		example(
		  ui, "An Expander with interactive content.",
		  xtk_expander(
			ui, "This Expander is bound to the model",
			(XtkExpanderDesc) {.expanded = m->expanded, .toggle = xtk_msg(MSG_EXPAND)},
			xtk_kids(
			  xtk_text(ui, "Anything can live in here — it is a normal container.", (XtkTextDesc) {0}),
			  xtk_checkbox(
				ui, "Even this checkbox from the CheckBox page",
				(XtkToggleDesc) {.checked = m->check_a, .change = xtk_msg(MSG_CHECK_A)}
			  )
			)
		  )
		)
	  )
	);
}
