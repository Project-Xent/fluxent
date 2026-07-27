#include "gallery.h"

XtkEl *page_button(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Button",
		  "The Button control provides a Click event to respond to user input. "
		  "It posts an XtkMsg; nothing calls back into your code."
		),
		example(
		  ui, "A simple Button with text content.",
		  demo_card(
			ui,
			xtk_button(
			  ui, "Standard button", (XtkButtonDesc) {.click = xtk_msg(MSG_CLICK), .disabled = m->button_disabled}
			),
			xtk_fmt(ui, "Clicked %d time%s", m->clicks, m->clicks == 1 ? "" : "s"),
			xtk_checkbox(
			  ui, "Disable button",
			  (XtkToggleDesc) {.checked = m->button_disabled, .change = xtk_msg(MSG_BUTTON_DISABLE)}
			)
		  )
		),
		example(
		  ui, "Built-in styles applied to Button.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				xtk_button(ui, "Accent style button", (XtkButtonDesc) {.style = XTK_BUTTON_ACCENT}),
				xtk_button(ui, "Standard style button", (XtkButtonDesc) {0}),
				xtk_button(ui, "Subtle style button", (XtkButtonDesc) {.style = XTK_BUTTON_SUBTLE}),
				xtk_button(ui, "Text style button", (XtkButtonDesc) {.style = XTK_BUTTON_TEXT})
			  )
			))
		  )
		),
		example(
		  ui, "A Button with graphical content.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				xtk_button(ui, "Save", (XtkButtonDesc) {.icon = "Save"}),
				xtk_button(ui, "Share", (XtkButtonDesc) {.icon = "Share"}),
				xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Mail"})
			  )
			))
		  )
		)
	  )
	);
}
