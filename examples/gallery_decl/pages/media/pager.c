#include "gallery.h"

XtkEl *page_pager(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "PagerControl",
		  "A numeric/arrow pager (distinct from the dot PipsPager): "
		  "First/Previous/Next/Last buttons around a number panel with ellipses, "
		  "or a compact page-of-total label."
		),
		example(
		  ui, "Number panel (32 pages) with First/Last buttons.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_START},
			xtk_kids(
			  xtk_pager(
				ui,
				(XtkPagerDesc) {
				  .nitem        = 32,
				  .sel          = m->pager_page,
				  .first_button = XTK_PAGER_BUTTON_AUTO,
				  .last_button  = XTK_PAGER_BUTTON_AUTO,
				  .select       = xtk_msg(MSG_PAGER_SELECT),
				}
			  ),
			  xtk_text(ui, xtk_fmt(ui, "Page %d of 32", m->pager_page + 1), (XtkTextDesc) {.size = 13})
			)
		  )
		),
		example(
		  ui, "Number-text display.",
		  xtk_pager(
			ui, (XtkPagerDesc) {
				  .nitem        = 32,
				  .sel          = m->pager_page,
				  .display_mode = XTK_PAGER_DISPLAY_NUMBER_TEXT,
				  .prefix       = "Page",
				  .suffix       = "of",
				  .select       = xtk_msg(MSG_PAGER_SELECT),
				}
		  )
		)
	  )
	);
}
