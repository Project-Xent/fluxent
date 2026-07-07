#include "gallery.h"

XtkEl *page_infobar(XtkUi *ui, Model const *m) {
	static char const *const kTitles [BAR_COUNT] = {"Informational", "Success", "Warning", "Error"};
	XtkEl                 **bars                = xtk_children_alloc(ui, BAR_COUNT);
	for (int i = 0; i < BAR_COUNT; i++) {
		if (!m->bar_open [i]) continue;
		bars [i] = xtk_keyed(
		  ui, xtk_fmt(ui, "bar-%d", i),
		  xtk_info_bar(
			ui, kTitles [i], "Essential app message; closable like the real control.",
			(XtkInfoBarDesc) {
			  .severity = ( FluxInfoBarSeverity ) i, .closable = true, .on_close = xtk_msg_i(MSG_BAR_CLOSE, i)}
		  )
		);
	}

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "InfoBar", "Use an InfoBar for essential status messages. Closing one just flips a model flag."
		),
		example(
		  ui, "One bar per severity; close them, then restore.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_column(ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH}, bars),
			  xtk_align(
				xtk_button(ui, "Restore all info bars", (XtkButtonDesc) {.on_click = xtk_msg(MSG_BARS_RESET)}),
				XENT_FLEX_ALIGN_START
			  ))
		  )
		))
	);
}

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
			  ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_CENTER},
			  xtk_kids(
				xtk_badge(ui, (XtkBadgeDesc) {0}),
				xtk_badge(ui, (XtkBadgeDesc) {.mode = FLUX_BADGE_NUMBER, .value = m->badge_value}),
				xtk_badge(ui, (XtkBadgeDesc) {.mode = FLUX_BADGE_ICON, .icon = "Mail"}),
				xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Add", .on_click = xtk_msg_i(MSG_BADGE_DELTA, 1)}),
				xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Minus", .on_click = xtk_msg_i(MSG_BADGE_DELTA, -1)}))
			),
			xtk_fmt(ui, "Badge value: %d", m->badge_value), NULL
		  )
		))
	);
}

XtkEl *page_progress(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Progress", "ProgressBar and ProgressRing show determinate progress or indeterminate activity."
		),
		example(
		  ui, "Bar and ring bound to the same slider.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 16},
			xtk_kids(
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .fill = true},
				xtk_kids(
				  xtk_sized(
					xtk_slider(ui, (XtkSliderDesc) {.value = m->progress, .on_change = xtk_msg(MSG_PROGRESS)}), 300, NAN
				  ),
				  xtk_text(ui, xtk_fmt(ui, "%.0f%%", m->progress), (XtkTextDesc) {0}), xtk_spacer(ui),
				  xtk_checkbox(
					ui, "Indeterminate",
					(XtkToggleDesc) {.checked = m->indeterminate, .on_change = xtk_msg(MSG_INDETERMINATE)}
				  ))
			  ),
			  xtk_progress(ui, (XtkProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate}),
			  xtk_progress_ring(ui, (XtkProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate}))
		  )
		))
	);
}

XtkEl *page_tooltip(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ToolTip", "Attach a tooltip to any element with xtk_tooltip; it shows after a short hover."
		),
		example(
		  ui, "A Button with a ToolTip.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				xtk_kids(
				  xtk_tooltip(
					ui, xtk_button(ui, "Hover me", (XtkButtonDesc) {0}), "A simple ToolTip on a Button"
				  ),
				  xtk_tooltip(
					ui, xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Save"}),
					"Icon-only buttons especially need one"
				  ))
			  ))
		  )
		))
	);
}
