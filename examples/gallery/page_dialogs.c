#include "gallery.h"

XtkEl *page_dialog(XtkUi *ui, Model const *m) {
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	char const *result = m->dialog_result >= 0 && m->dialog_result <= 2 ? kResults [m->dialog_result] : "(none yet)";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "ContentDialog", "A modal dialog controlled by the model: open while a flag is true."
		),
		example(
		  ui, "A dialog with Delete / Cancel buttons.",
		  demo_card(
			ui, xtk_button(ui, "Show dialog", (XtkButtonDesc) {.on_click = xtk_msg(MSG_DIALOG_OPEN)}),
			xtk_fmt(ui, "Last result: %s", result), NULL
		  )
		),
		xtk_content_dialog(
		  ui, "Delete file?",
		  (XtkDialogDesc) {
			.open           = m->dialog_open,
			.primary        = "Delete",
			.close          = "Cancel",
			.width          = 400,
			.content_height = 56,
			.on_result      = xtk_msg(MSG_DIALOG_RESULT)},
		  (XtkEl *[]) {
			xtk_text(
			  ui, "This file will be permanently removed. This cannot be undone.", (XtkTextDesc) {.size = 14}
			),
			XTK_END}
		),
		XTK_END}
	);
}

XtkEl *page_settings(XtkUi *ui, Model const *m) {
	( void ) m;
	bool dark = flux_theme_system_is_dark();
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(ui, "Settings", "About this gallery and its environment."),
		xtk_card(
		  ui, (XtkStackDesc) {0},
		  (XtkEl *[]) {
			xtk_text(ui, "App theme", (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(
			  ui, xtk_fmt(ui, "Following the system theme: %s (set at startup).", dark ? "Dark" : "Light"),
			  (XtkTextDesc) {.size = 13}
			),
			XTK_END}
		),
		xtk_card(
		  ui, (XtkStackDesc) {0},
		  (XtkEl *[]) {
			xtk_text(ui, "About", (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(
			  ui, "FluXent Gallery — fluxent 0.1.0, declarative layer over retained controls.",
			  (XtkTextDesc) {.size = 13}
			),
			xtk_hyperlink(ui, "Source code", (XtkHyperlinkDesc) {.url = "https://github.com/Project-Xent"}), XTK_END}
		),
		XTK_END}
	);
}
