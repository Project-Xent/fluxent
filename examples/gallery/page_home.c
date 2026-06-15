#include "gallery.h"

static FluxEl *home_card(FxUi *ui, char const *icon, char const *title, char const *body) {
	return fx_grow(
	  fx_card(
		ui, (FxStackDesc) {.gap = 8},
		(FluxEl *[]) {
		  fx_button(ui, NULL, (FxButtonDesc) {.style = FLUX_BUTTON_SUBTLE, .icon = icon, .disabled = true}),
		  fx_text(ui, title, (FxTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
		  fx_text(ui, body, (FxTextDesc) {.size = 13}), FX_END}
	  ),
	  1.0f
	);
}

FluxEl *page_home(FxUi *ui, Model const *m) {
	( void ) m;
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "FluXent Gallery",
		  "WinUI 3-style controls in pure C — every page is a live FX view function over one shared model."
		),
		fx_info_bar(
		  ui, "Welcome", "Pick a control in the navigation pane; everything you see is reconciled per frame.",
		  (FxInfoBarDesc) {.severity = FLUX_INFOBAR_SUCCESS}
		),
		fx_row(
		  ui, (FxStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
		  (FluxEl *[]) {
			home_card(ui, "Send", "Elm loop", "Controls post messages; update() evolves the model; view() rebuilds."),
			home_card(ui, "Sync", "Keyed diff", "The reconciler diffs the element tree and patches retained controls."),
			home_card(ui, "Edit", "Plain C17", "Designated initializers + compound literals; zero is the default."),
			FX_END}
		),
		FX_END}
	);
}
