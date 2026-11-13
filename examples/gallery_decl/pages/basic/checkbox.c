#include "gallery.h"

XtkEl *page_checkbox(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "CheckBox", "CheckBox controls let the user select a combination of binary options."),
		example(
		  ui, "A 2-state CheckBox.",
		  demo_card(
			ui,
			xtk_checkbox(
			  ui, "Two-state CheckBox", (XtkToggleDesc) {.checked = m->check_a, .change = xtk_msg(MSG_CHECK_A)}
			),
			m->check_a ? "Checked" : "Unchecked", NULL
		  )
		),
		example(
		  ui, "A 3-state CheckBox: a click cycles unchecked, checked, indeterminate.",
		  demo_card(
			ui,
			xtk_checkbox(
			  ui, "Three-state CheckBox",
			  (XtkToggleDesc) {
				.checked       = m->check_state == XTK_CHECK_CHECKED,
				.indeterminate = m->check_state == XTK_CHECK_INDETERMINATE,
				.three_state   = true,
				.change_state  = xtk_msg(MSG_CHECK_TRI)}
			),
			m->check_state == XTK_CHECK_CHECKED         ? "Checked"
			: m->check_state == XTK_CHECK_INDETERMINATE ? "Indeterminate"
			                                            : "Unchecked",
			NULL
		  )
		),
		example(
		  ui, "CheckBoxes can be disabled.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_checkbox(
				ui, "Enabled option", (XtkToggleDesc) {.checked = m->check_b, .change = xtk_msg(MSG_CHECK_B)}
			  ),
			  xtk_checkbox(ui, "Disabled option", (XtkToggleDesc) {.disabled = true}),
			  xtk_checkbox(ui, "Disabled and checked", (XtkToggleDesc) {.checked = true, .disabled = true})
			)
		  )
		)
	  )
	);
}
