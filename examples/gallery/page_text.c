#include "gallery.h"

XtkEl *page_textbox(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "TextBox", "A TextBox posts its content on every edit; the model decides what to keep."),
		example(
		  ui, "A simple TextBox.",
		  demo_card(
			ui, xtk_sized(xtk_textbox(ui, "Enter your name", (XtkTextBoxDesc) {.on_change = xtk_msg(MSG_NAME)}), 300, NAN),
			m->name && m->name [0] ? xtk_fmt(ui, "Hello, %s!", m->name) : "Waiting for input…", NULL
		  )
		),
		example(
		  ui, "A PasswordBox hides what you type.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_password(ui, "Enter a password", (XtkTextBoxDesc) {.on_change = xtk_msg(MSG_PASSWORD)}), 300, NAN
			),
			xtk_fmt(ui, "%d character%s", m->pass_chars, m->pass_chars == 1 ? "" : "s"), NULL
		  )
		))
	);
}

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
			xtk_number_box(
			  ui, (XtkNumberDesc) {.max = 100, .step = 1, .value = m->quantity, .on_change = xtk_msg(MSG_QUANTITY)}
			),
			xtk_fmt(ui, "Quantity: %.0f", m->quantity), NULL
		  )
		))
	);
}

XtkEl *page_typography(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Typography", "The WinUI 3 type ramp rendered by xtk_text."),
		xtk_card(
		  ui, (XtkStackDesc) {.gap = 10},
		  xtk_kids(
			xtk_text(ui, "Caption — 12", (XtkTextDesc) {.size = 12}),
			xtk_text(ui, "Body — 14", (XtkTextDesc) {.size = 14}),
			xtk_text(ui, "Body Strong — 14 SemiBold", (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Body Large — 18", (XtkTextDesc) {.size = 18}),
			xtk_text(ui, "Subtitle — 20 SemiBold", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title — 28 SemiBold", (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title Large — 40 SemiBold", (XtkTextDesc) {.size = 40, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Display — 68 SemiBold", (XtkTextDesc) {.size = 68, .weight = FLUX_FONT_SEMI_BOLD}))
		))
	);
}
