#include "gallery.h"

XtkEl *page_typography(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Typography", "The WinUI 3 type ramp rendered by xtk_text."),
		xtk_card(
		  ui, (XtkStackDesc) {.gap = 10},
		  xtk_kids(
			xtk_text(ui, "Caption — 12", (XtkTextDesc) {.size = 12}),
			xtk_text(ui, "Body — 14", (XtkTextDesc) {.size = 14}),
			xtk_text(ui, "Body Strong — 14 SemiBold", (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Body Large — 18", (XtkTextDesc) {.size = 18}),
			xtk_text(ui, "Subtitle — 20 SemiBold", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title — 28 SemiBold", (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title Large — 40 SemiBold", (XtkTextDesc) {.size = 40, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Display — 68 SemiBold", (XtkTextDesc) {.size = 68, .weight = FLUX_FONT_SEMI_BOLD})
		  )
		)
	  )
	);
}
