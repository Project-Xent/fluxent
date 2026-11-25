#include "gallery.h"

static XtkEl *titlebar_content_demo(XtkUi *ui, bool visible) {
	XtkEl *left    = visible ? xtk_text(ui, "Left header", (XtkTextDesc) {.size = 12}) : NULL;
	XtkEl *content = visible ? xtk_button(ui, "Remove slots", (XtkButtonDesc) {.click = xtk_msg(MSG_TB_PANE)}) : NULL;
	XtkEl *right   = visible ? xtk_text(ui, "Right header", (XtkTextDesc) {.size = 12}) : NULL;
	return xtk_titlebar_slots(ui, "FluXent Gallery", (XtkTitleBarDesc) {.subtitle = "Preview"}, left, content, right);
}

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
		  ui, "Content, left header, and right header",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_button(ui, "Toggle title bar slots", (XtkButtonDesc) {.click = xtk_msg(MSG_TB_PANE)}),
			  titlebar_content_demo(ui, (m->tb_pane_clicks & 1) != 0)
			)
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
