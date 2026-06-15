#include "gallery.h"

FluxEl *page_infobar(FxUi *ui, Model const *m) {
	static char const *const kTitles [BAR_COUNT] = {"Informational", "Success", "Warning", "Error"};
	FluxEl                 **bars                = fx_children_alloc(ui, BAR_COUNT);
	for (int i = 0; i < BAR_COUNT; i++) {
		if (!m->bar_open [i]) continue;
		bars [i] = fx_keyed(
		  ui, fx_fmt(ui, "bar-%d", i),
		  fx_info_bar(
			ui, kTitles [i], "Essential app message; closable like the real control.",
			(FxInfoBarDesc) {
			  .severity = ( FluxInfoBarSeverity ) i, .closable = true, .on_close = fx_msg_i(MSG_BAR_CLOSE, i)}
		  )
		);
	}

	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "InfoBar", "Use an InfoBar for essential status messages. Closing one just flips a model flag."
		),
		example(
		  ui, "One bar per severity; close them, then restore.",
		  fx_column(
			ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
			(FluxEl *[]) {
			  fx_column(ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH}, bars),
			  fx_align(
				fx_button(ui, "Restore all info bars", (FxButtonDesc) {.on_click = fx_msg(MSG_BARS_RESET)}),
				XENT_FLEX_ALIGN_START
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}

FluxEl *page_badge(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "InfoBadge", "InfoBadges are non-intrusive notification indicators: dot, number, or icon."),
		example(
		  ui, "Dot, numeric, and icon badges sharing one counter.",
		  demo_card(
			ui,
			fx_row(
			  ui, (FxStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_CENTER},
			  (FluxEl *[]) {
				fx_badge(ui, (FxBadgeDesc) {0}),
				fx_badge(ui, (FxBadgeDesc) {.mode = FLUX_BADGE_NUMBER, .value = m->badge_value}),
				fx_badge(ui, (FxBadgeDesc) {.mode = FLUX_BADGE_ICON, .icon = "Mail"}),
				fx_button(ui, NULL, (FxButtonDesc) {.icon = "Add", .on_click = fx_msg_i(MSG_BADGE_DELTA, 1)}),
				fx_button(ui, NULL, (FxButtonDesc) {.icon = "Minus", .on_click = fx_msg_i(MSG_BADGE_DELTA, -1)}),
				FX_END}
			),
			fx_fmt(ui, "Badge value: %d", m->badge_value), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_progress(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "Progress", "ProgressBar and ProgressRing show determinate progress or indeterminate activity."
		),
		example(
		  ui, "Bar and ring bound to the same slider.",
		  fx_card(
			ui, (FxStackDesc) {.gap = 16},
			(FluxEl *[]) {
			  fx_row(
				ui, (FxStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .fill = true},
				(FluxEl *[]) {
				  fx_sized(
					fx_slider(ui, (FxSliderDesc) {.value = m->progress, .on_change = fx_msg(MSG_PROGRESS)}), 300, NAN
				  ),
				  fx_text(ui, fx_fmt(ui, "%.0f%%", m->progress), (FxTextDesc) {0}), fx_spacer(ui),
				  fx_checkbox(
					ui, "Indeterminate",
					(FxToggleDesc) {.checked = m->indeterminate, .on_change = fx_msg(MSG_INDETERMINATE)}
				  ),
				  FX_END}
			  ),
			  fx_progress(ui, (FxProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate}),
			  fx_progress_ring(ui, (FxProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate}), FX_END}
		  )
		),
		FX_END}
	);
}

FluxEl *page_tooltip(FxUi *ui, Model const *m) {
	( void ) m;
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "ToolTip", "Attach a tooltip to any element with fx_tooltip; it shows after a short hover."
		),
		example(
		  ui, "A Button with a ToolTip.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_row(
				ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				(FluxEl *[]) {
				  fx_tooltip(
					ui, fx_button(ui, "Hover me", (FxButtonDesc) {0}), "A simple ToolTip on a Button"
				  ),
				  fx_tooltip(
					ui, fx_button(ui, NULL, (FxButtonDesc) {.icon = "Save"}),
					"Icon-only buttons especially need one"
				  ),
				  FX_END}
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}
