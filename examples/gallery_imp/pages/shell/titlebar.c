#include "app.h"

static char const *clicks_text(GalleryApp *g) {
	return gallery_fmt(
	  g, "Back clicks: %d.  Pane-toggle clicks: %d.", g->model.tb_back_clicks, g->model.tb_pane_clicks
	);
}

static void title_bar_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_TB_BACK], clicks_text(g)); }

bool        page_title_bar_build(PageBuild *b) {
	page_sync(b, title_bar_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "TitleBar",
	  "A window title-bar band: an app icon, title and subtitle, plus optional back and pane-toggle buttons. Opt into "
	  "window integration to make the band drag the window."
	);
	XtkNodeRef card = xtk_ref_local(ui_card(
	  &b->txn, page_example(b, parent, "Icon, title, subtitle, back and pane-toggle buttons."), 0.0f,
	  XENT_FLEX_ALIGN_STRETCH, 0.0f
	));
	XtkLocalId bar  = xtk_titlebar_create(
	  &b->txn, card,
	  &(XtkTitleBarProps) {
		.set        = XTK_TITLEBAR_TEXT_MASK
	                | XTK_TITLEBAR_SUBTITLE_MASK
	                | XTK_TITLEBAR_ICON_MASK
	                | XTK_TITLEBAR_SHOWBACK_MASK
	                | XTK_TITLEBAR_SHOWTOGGLE_MASK,
		.text       = "FluXent Gallery",
		.subtitle   = "Preview",
		.icon       = "Home",
		.showback   = true,
		.showtoggle = true,
	  }
	);
	xtk_titlebar_oninvoke(&b->txn, xtk_ref_local(bar), page_post, page_bind(b, MSG_TB_BACK));
	page_on(b, bar, XTK_EVENT_TOGGLE, MSG_TB_PANE, 0);
	XtkNodeRef events
	  = xtk_ref_local(ui_card(&b->txn, page_example(b, parent, "Button events."), 0.0f, XENT_FLEX_ALIGN_START, 16.0f));
	page_keepout(b, MSG_TB_BACK, page_note(b, events, clicks_text(b->app)));
	return true;
}
