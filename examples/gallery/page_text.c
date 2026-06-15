#include "gallery.h"

FluxEl *page_textbox(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "TextBox", "A TextBox posts its content on every edit; the model decides what to keep."),
		example(
		  ui, "A simple TextBox.",
		  demo_card(
			ui, fx_sized(fx_textbox(ui, "Enter your name", (FxTextBoxDesc) {.on_change = fx_msg(MSG_NAME)}), 300, NAN),
			m->name [0] ? fx_fmt(ui, "Hello, %s!", m->name) : "Waiting for input…", NULL
		  )
		),
		example(
		  ui, "A PasswordBox hides what you type.",
		  demo_card(
			ui,
			fx_sized(
			  fx_password(ui, "Enter a password", (FxTextBoxDesc) {.on_change = fx_msg(MSG_PASSWORD)}), 300, NAN
			),
			fx_fmt(ui, "%d character%s", m->pass_chars, m->pass_chars == 1 ? "" : "s"), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_numberbox(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "NumberBox", "Use a NumberBox for numeric input with spin buttons, wheel, and keyboard support."
		),
		example(
		  ui, "A NumberBox with spin buttons (0–100).",
		  demo_card(
			ui,
			fx_number_box(
			  ui, (FxNumberDesc) {.max = 100, .step = 1, .value = m->quantity, .on_change = fx_msg(MSG_QUANTITY)}
			),
			fx_fmt(ui, "Quantity: %.0f", m->quantity), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_typography(FxUi *ui, Model const *m) {
	( void ) m;
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "Typography", "The WinUI 3 type ramp rendered by fx_text."),
		fx_card(
		  ui, (FxStackDesc) {.gap = 10},
		  (FluxEl *[]) {
			fx_text(ui, "Caption — 12", (FxTextDesc) {.size = 12}),
			fx_text(ui, "Body — 14", (FxTextDesc) {.size = 14}),
			fx_text(ui, "Body Strong — 14 SemiBold", (FxTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(ui, "Body Large — 18", (FxTextDesc) {.size = 18}),
			fx_text(ui, "Subtitle — 20 SemiBold", (FxTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(ui, "Title — 28 SemiBold", (FxTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(ui, "Title Large — 40 SemiBold", (FxTextDesc) {.size = 40, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(ui, "Display — 68 SemiBold", (FxTextDesc) {.size = 68, .weight = FLUX_FONT_SEMI_BOLD}), FX_END}
		),
		FX_END}
	);
}
