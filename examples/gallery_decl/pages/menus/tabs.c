#include "gallery.h"
#include <stdio.h>

static XtkEl *tab_content(XtkUi *ui, Model const *m, int sel) {
	if (m->ntab == 0) return NULL;
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 8, .padding = {16, 16, 16, 16}
    },
	  xtk_kids(
		xtk_text(
		  ui, xtk_fmt(ui, "Document %d", m->tab_ids [sel]), (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}
		),
		xtk_text(
		  ui, "This page is a function of the model's selection — the same view subtree follows it.",
		  (XtkTextDesc) {.size = 13}
		)
	  )
	);
}

XtkEl *page_tabview(XtkUi *ui, Model const *m) {
	XtkTabDesc tabs [8];
	int        count = m->ntab;
	for (int i = 0; i < count; i++)
		tabs [i] = (XtkTabDesc) {.icon = "OpenFile", .label = xtk_fmt(ui, "Document %d", m->tab_ids [i])};
	int sel = (m->tab_sel >= 0 && m->tab_sel < count) ? m->tab_sel : 0;

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "TabView", "Tabs live in the model: selection, close, and add update the retained control."),
		example(
		  ui, "A TabView with closable tabs and an add button.",
		  xtk_sized(
			xtk_tabs(
			  ui,
			  (XtkTabViewDesc) {
				.tabs   = tabs,
				.ntab   = count,
				.sel    = sel,
				.select = xtk_msg(MSG_TAB_SELECT),
				.close  = xtk_msg(MSG_TAB_CLOSE),
				.add    = xtk_msg(MSG_TAB_ADD)},
			  xtk_kids(tab_content(ui, m, sel))
			),
			NAN, 280
		  )
		)
	  )
	);
}
