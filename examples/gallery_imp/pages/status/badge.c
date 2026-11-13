#include "app.h"

#include <string.h>

static void       page_badge_sync(GalleryApp *g);

static XtkLocalId badge_button(PageBuild *b, XtkNodeRef parent, char const *icon, int delta) {
	XtkLocalId button = xtk_button_create(
	  &b->txn, parent,
	  &(XtkButtonProps) {
		.set     = XTK_BUTTON_ICON_MASK | XTK_BUTTON_STYLE_MASK | XTK_BUTTON_ENABLED_MASK,
		.icon    = icon,
		.style   = XTK_BUTTON_STANDARD,
		.enabled = true,
	  }
	);
	xtk_button_oninvoke(&b->txn, xtk_ref_local(button), page_post, page_bind_i(b, MSG_BADGE_DELTA, delta));
	return button;
}

bool page_badge_build(PageBuild *b) {
	page_sync(b, page_badge_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);

	page_header(b, parent, "InfoBadge", "InfoBadges are non-intrusive notification indicators: dot, number, or icon.");
	XtkNodeRef ex   = page_example(b, parent, "Dot, numeric, and icon badges sharing one counter.");
	PageDemo   demo = page_demo(b, ex);
	xtk_badge_create(
	  &b->txn, demo.items,
	  &(XtkBadgeProps) {
		.set       = XTK_BADGE_BADGEMODE_MASK,
		.badgemode = XTK_BADGE_DOT,
	  }
	);
	XtkLocalId numeric = xtk_badge_create(
	  &b->txn, demo.items,
	  &(XtkBadgeProps) {
		.set       = XTK_BADGE_BADGEMODE_MASK | XTK_BADGE_VALUE_MASK,
		.badgemode = XTK_BADGE_NUMBER,
		.value     = m->badge_value,
	  }
	);
	xtk_badge_create(
	  &b->txn, demo.items,
	  &(XtkBadgeProps) {
		.set       = XTK_BADGE_BADGEMODE_MASK | XTK_BADGE_ICON_MASK,
		.badgemode = XTK_BADGE_ICON,
		.icon      = "Mail",
	  }
	);
	badge_button(b, demo.items, "Add", 1);
	badge_button(b, demo.items, "Minus", -1);
	page_keep(b, MSG_BADGE_DELTA, numeric);
	XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "Badge value: %d", m->badge_value));
	page_keepout(b, MSG_BADGE_DELTA, output);
	return true;
}

static void page_badge_sync(GalleryApp *g) {
	xtk_badge_setvalue(
	  &g->scene, ( XtkCtrl ) g->live_control [MSG_BADGE_DELTA], g->model.badge_value, (XtkChangeStamp) {0}
	);
	char const *text = gallery_fmt(g, "Badge value: %d", g->model.badge_value);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_BADGE_DELTA], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}
