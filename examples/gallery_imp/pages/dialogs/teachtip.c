#include "app.h"

static char const *events_text(GalleryApp *g) {
	return gallery_fmt(g, "Action clicks: %d. Last close reason: %d.", g->model.tip_actions, g->model.tip_close_reason);
}

static void tip_sync(GalleryApp *g) {
	bool open = g->model.tip_open;
	xtk_teachtip_setopen(&g->scene, g->live_control [MSG_TIP_OPEN], open, (XtkChangeStamp) {0});
	xtk_button_settext(&g->scene, g->live_extra [0], open ? "Hide tip" : "Show tip", 0, (XtkChangeStamp) {0});
	gallery_settext(g, g->live_output [MSG_TIP_OPEN], open ? "Tip is open." : "Tip is closed.");
	gallery_settext(g, g->live_output [MSG_TIP_ACTION], events_text(g));
}

bool page_teaching_tip_build(PageBuild *b) {
	page_sync(b, tip_sync);
	bool       open   = b->app->model.tip_open;
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "TeachingTip", "A lightweight teaching surface with subtitle, action, and close affordances."
	);
	PageDemo demo = page_demo(b, page_example(b, parent, "Untargeted tip toggled from the model."));
	page_keepx(b, 0, page_button(b, demo.items, open ? "Hide tip" : "Show tip", MSG_TIP_OPEN, 0));
	page_keepout(b, MSG_TIP_OPEN, page_output(b, demo.row, open ? "Tip is open." : "Tip is closed."));
	XtkLocalId tip = xtk_teachtip_create(
	  &b->txn, parent,
	  &(XtkTeachingTipProps) {
		.set          = XTK_TEACHTIP_TEXT_MASK
	                  | XTK_TEACHTIP_SUBTITLE_MASK
	                  | XTK_TEACHTIP_ICON_MASK
	                  | XTK_TEACHTIP_ACTION_MASK
	                  | XTK_TEACHTIP_ACTIONACCENT_MASK
	                  | XTK_TEACHTIP_UNTARGETED_MASK
	                  | XTK_TEACHTIP_OPEN_MASK,
		.text         = "Welcome to FluXent",
		.subtitle     = "Explore the WinUI 3 control gallery.",
		.icon         = "Lightbulb",
		.action       = "Got it",
		.actionaccent = true,
		.untargeted   = true,
		.open         = open,
	  }
	);
	xtk_teachtip_oninvoke(&b->txn, xtk_ref_local(tip), page_post, page_bind(b, MSG_TIP_ACTION));
	xtk_teachtip_onclose(&b->txn, xtk_ref_local(tip), page_post, page_bind(b, MSG_TIP_CLOSE));
	page_keep(b, MSG_TIP_OPEN, tip);
	XtkNodeRef card
	  = xtk_ref_local(ui_card(&b->txn, page_example(b, parent, "Tip events."), 0.0f, XENT_FLEX_ALIGN_START, 16.0f));
	page_keepout(b, MSG_TIP_ACTION, page_note(b, card, events_text(b->app)));
	return true;
}
