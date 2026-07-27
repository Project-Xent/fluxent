#include "gallery.h"

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
			ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_row(
				ui,
				(XtkStackDesc) {
				  .gap     = 16,
				  .align   = XENT_FLEX_ALIGN_CENTER,
				  .justify = XENT_FLEX_JUSTIFY_SPACE_BETWEEN,
				  .fill    = true,
				  .wrap    = XENT_FLEX_WRAP},
				xtk_kids(
				  xtk_sized(
					xtk_slider(ui, (XtkSliderDesc) {.value = m->progress, .change = xtk_msg(MSG_PROGRESS)}), 300, NAN
				  ),
				  xtk_text(ui, xtk_fmt(ui, "%.0f%%", m->progress), (XtkTextDesc) {0}),
				  xtk_checkbox(
					ui, "Indeterminate",
					(XtkToggleDesc) {.checked = m->indeterminate, .change = xtk_msg(MSG_INDETERMINATE)}
				  )
				)
			  ),
			  xtk_progress(ui, (XtkProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate}),
			  xtk_progressring(ui, (XtkProgressDesc) {.value = m->progress, .indeterminate = m->indeterminate})
			)
		  )
		)
	  )
	);
}
