#include "gallery.h"

static XtkEl *home_card(XtkUi *ui, char const *icon, char const *title, char const *body) {
	return xtk_grow(
	  xtk_card(
		ui, (XtkStackDesc) {.gap = 8},
		(XtkEl *[]) {
		  xtk_button(ui, NULL, (XtkButtonDesc) {.style = FLUX_BUTTON_SUBTLE, .icon = icon, .disabled = true}),
		  xtk_text(ui, title, (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, body, (XtkTextDesc) {.size = 13}), XTK_END}
	  ),
	  1.0f
	);
}

XtkEl *page_home(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "FluXent Gallery",
		  "WinUI 3-style controls in pure C — every page is a live view function over one shared model."
		),
		xtk_info_bar(
		  ui, "Welcome", "Pick a control in the navigation pane; everything you see is reconciled per frame.",
		  (XtkInfoBarDesc) {.severity = FLUX_INFOBAR_SUCCESS}
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
		  (XtkEl *[]) {
			home_card(ui, "Send", "Elm loop", "Controls post messages; update() evolves the model; view() rebuilds."),
			home_card(ui, "Sync", "Keyed diff", "The reconciler diffs the element tree and patches retained controls."),
			home_card(ui, "Edit", "Plain C17", "Designated initializers + compound literals; zero is the default."),
			XTK_END}
		),
		XTK_END}
	);
}
