#include "gallery.h"

XtkEl *page_teaching_tip(XtkUi *ui, Model const *m) {
	char const *status = m->tip_open ? "Tip is open." : "Tip is closed.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "TeachingTip", "A lightweight teaching surface with subtitle, action, and close affordances."),
		example(
		  ui, "Untargeted tip toggled from the model.",
		  demo_card(
			ui, xtk_button(ui, m->tip_open ? "Hide tip" : "Show tip", (XtkButtonDesc) {.click = xtk_msg(MSG_TIP_OPEN)}),
			status, NULL
		  )
		),
		xtk_teachtip(
		  ui, "Welcome to FluXent",
		  (XtkTeachingTipDesc) {
			.open          = m->tip_open,
			.subtitle      = "Explore the WinUI 3 control gallery.",
			.icon          = "Lightbulb",
			.action_text   = "Got it",
			.action_accent = true,
			.untargeted    = true,
			.on_action     = xtk_msg(MSG_TIP_ACTION),
			.close         = xtk_msg(MSG_TIP_CLOSE),
		  },
		  xtk_kids()
		),
		example(
		  ui, "Tip events.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_text(
			  ui, xtk_fmt(ui, "Action clicks: %d. Last close reason: %d.", m->tip_actions, m->tip_close_reason),
			  (XtkTextDesc) {.size = 13}
			))
		  )
		)
	  )
	);
}
