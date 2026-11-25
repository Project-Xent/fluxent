#include "gallery.h"

static char const *const kNavStyles [] = {"Left", "Top"};

XtkEl                   *page_settings(XtkUi *ui, Model const *m) {
	bool dark = flux_theme_systemdark();
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Settings", "About this gallery and its environment."),
		xtk_card(
		  ui, (XtkStackDesc) {0},
		  xtk_kids(
			xtk_text(ui, "App theme", (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(
			  ui, xtk_fmt(ui, "Following the system theme: %s (set at startup).", dark ? "Dark" : "Light"),
			  (XtkTextDesc) {.size = 13}
			)
		  )
		),
		xtk_card(
		  ui, (XtkStackDesc) {0},
		  xtk_kids(
			xtk_text(ui, "Navigation style", (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_combo(
			  ui, "Navigation style",
			  (XtkComboDesc) {.items = kNavStyles, .nitem = 2, .sel = m->nav_style, .select = xtk_msg(MSG_NAV_STYLE)}
			)
		  )
		),
		xtk_card(
		  ui, (XtkStackDesc) {0},
		  xtk_kids(
			xtk_text(ui, "About", (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(
			  ui, "FluXent Gallery — fluxent " GALLERY_VERSION ", declarative layer over retained controls.",
			  (XtkTextDesc) {.size = 13}
			),
			xtk_link(ui, "Source code", (XtkHyperlinkDesc) {.url = "https://github.com/Project-Xent"})
		  )
		)
	  )
	);
}
