#include "gallery.h"

XtkEl *page_title_bar(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH
    },
	  xtk_kids(
		page_header(
		  ui, "TitleBar",
		  "A window title-bar band: an app icon, title and subtitle, plus optional "
		  "back and pane-toggle buttons. Opt into window integration to make the "
		  "band drag the window."
		),
		example(
		  ui, "Icon, title, subtitle, back and pane-toggle buttons.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 0, .align = XENT_FLEX_ALIGN_STRETCH, .padding = {0, 0, 0, 0}},
			xtk_kids(xtk_titlebar(
			  ui, "FluXent Gallery",
			  (XtkTitleBarDesc) {
				.subtitle         = "Preview",
				.icon             = "Home",
				.show_back        = true,
				.show_pane_toggle = true,
				.on_back          = xtk_msg(MSG_TB_BACK),
				.on_pane_toggle   = xtk_msg(MSG_TB_PANE),
			  }
			))
		  )
		),
		example(
		  ui, "Button events.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_text(
			  ui, xtk_fmt(ui, "Back clicks: %d.  Pane-toggle clicks: %d.", m->tb_back_clicks, m->tb_pane_clicks),
			  (XtkTextDesc) {.size = 13}
			))
		  )
		)
	  )
	);
}
