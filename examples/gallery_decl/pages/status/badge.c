#include "gallery.h"

XtkEl *page_badge(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "InfoBadge", "InfoBadges are non-intrusive notification indicators: dot, number, or icon."),
		example(
		  ui, "Dot, numeric, and icon badges sharing one counter.",
		  demo_card(
			ui,
			xtk_row(
			  ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				xtk_badge(ui, (XtkBadgeDesc) {0}),
				xtk_badge(ui, (XtkBadgeDesc) {.mode = XTK_BADGE_NUMBER, .value = m->badge_value}),
				xtk_badge(ui, (XtkBadgeDesc) {.mode = XTK_BADGE_ICON, .icon = "Mail"}),
				xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Add", .click = xtk_msg_i(MSG_BADGE_DELTA, 1)}),
				xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Minus", .click = xtk_msg_i(MSG_BADGE_DELTA, -1)})
			  )
			),
			xtk_fmt(ui, "Badge value: %d", m->badge_value), NULL
		  )
		)
	  )
	);
}
