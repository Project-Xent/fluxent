#include "gallery.h"

FluxEl *page_dialog(FxUi *ui, Model const *m) {
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	char const *result = m->dialog_result >= 0 && m->dialog_result <= 2 ? kResults [m->dialog_result] : "(none yet)";
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "ContentDialog", "A modal dialog controlled by the model: open while a flag is true."
		),
		example(
		  ui, "A dialog with Delete / Cancel buttons.",
		  demo_card(
			ui, fx_button(ui, "Show dialog", (FxButtonDesc) {.on_click = fx_msg(MSG_DIALOG_OPEN)}),
			fx_fmt(ui, "Last result: %s", result), NULL
		  )
		),
		fx_content_dialog(
		  ui, "Delete file?",
		  (FxDialogDesc) {
			.open           = m->dialog_open,
			.primary        = "Delete",
			.close          = "Cancel",
			.width          = 400,
			.content_height = 56,
			.on_result      = fx_msg(MSG_DIALOG_RESULT)},
		  (FluxEl *[]) {
			fx_text(
			  ui, "This file will be permanently removed. This cannot be undone.", (FxTextDesc) {.size = 14}
			),
			FX_END}
		),
		FX_END}
	);
}

FluxEl *page_settings(FxUi *ui, Model const *m) {
	( void ) m;
	bool dark = flux_theme_system_is_dark();
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "Settings", "About this gallery and its environment."),
		fx_card(
		  ui, (FxStackDesc) {0},
		  (FluxEl *[]) {
			fx_text(ui, "App theme", (FxTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(
			  ui, fx_fmt(ui, "Following the system theme: %s (set at startup).", dark ? "Dark" : "Light"),
			  (FxTextDesc) {.size = 13}
			),
			FX_END}
		),
		fx_card(
		  ui, (FxStackDesc) {0},
		  (FluxEl *[]) {
			fx_text(ui, "About", (FxTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			fx_text(
			  ui, "FluXent Gallery — fluxent 0.1.0, FX declarative layer over retained controls.",
			  (FxTextDesc) {.size = 13}
			),
			fx_hyperlink(ui, "Source code", (FxHyperlinkDesc) {.url = "https://github.com/Project-Xent"}), FX_END}
		),
		FX_END}
	);
}
