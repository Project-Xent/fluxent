#include "gallery.h"

XtkEl *page_infobar(XtkUi *ui, Model const *m) {
	static char const *const kTitles [BAR_COUNT] = {"Informational", "Success", "Warning", "Error"};
	XtkEl                  **bars                = xtk_children_alloc(ui, BAR_COUNT);
	for (int i = 0; i < BAR_COUNT; i++) {
		if (!m->bar_open [i]) continue;
		bars [i] = xtk_keyed(
		  ui, xtk_fmt(ui, "bar-%d", i),
		  xtk_infobar(
			ui, kTitles [i], "Essential app message; closable like the real control.",
			(XtkInfoBarDesc) {
			  .severity = ( XtkInfoBarSeverity ) i, .closable = true, .close = xtk_msg_i(MSG_BAR_CLOSE, i)}
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
				xtk_button(ui, "Restore all info bars", (XtkButtonDesc) {.click = xtk_msg(MSG_BARS_RESET)}),
				XENT_FLEX_ALIGN_START
			  )
			)
		  )
		)
	  )
	);
}
