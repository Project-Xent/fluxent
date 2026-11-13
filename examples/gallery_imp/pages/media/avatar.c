#include "app.h"

static XtkLocalId avatar(XtkTxn *txn, XtkNodeRef parent, XtkAvatarProps props) {
	float size  = props.w > 0.0f ? props.w : 72.0f;
	props.set  |= XTK_AVATAR_W_MASK | XTK_AVATAR_H_MASK;
	props.w     = size;
	props.h     = size;
	if (props.text) props.set |= XTK_AVATAR_TEXT_MASK;
	if (props.initials) props.set |= XTK_AVATAR_INITIALS_MASK;
	if (props.badgeglyph) props.set |= XTK_AVATAR_BADGEGLYPH_MASK;
	if (props.badgevalue) props.set |= XTK_AVATAR_BADGEVALUE_MASK;
	if (props.group) props.set |= XTK_AVATAR_GROUP_MASK;
	return xtk_avatar_create(txn, parent, &props);
}

static void labeled(XtkTxn *txn, XtkNodeRef parent, char const *caption, XtkAvatarProps props) {
	XtkNodeRef col = xtk_ref_local(ui_column(txn, parent, 8.0f, XENT_FLEX_ALIGN_CENTER));
	avatar(txn, col, props);
	ui_text(txn, col, caption, 12.0f, FLUX_FONT_REGULAR);
}

static void person_sync(GalleryApp *g) {
	XtkCtrl live = g->live_control [MSG_PP_BADGE];
	xtk_avatar_setbadgevalue(&g->scene, live, g->model.pp_badge, (XtkChangeStamp) {0});
	xtk_avatar_setgroup(&g->scene, live, g->model.pp_group, (XtkChangeStamp) {0});
	gallery_settext(g, g->live_output [MSG_PP_BADGE], gallery_fmt(g, "Badge: %d", g->model.pp_badge));
}

bool page_person_picture_build(PageBuild *b) {
	page_sync(b, person_sync);
	Model const *m      = &b->app->model;
	XtkTxn      *txn    = &b->txn;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "PersonPicture",
	  "A circular avatar that shows a profile photo, initials derived from a display name, or a placeholder glyph — "
	  "with an optional badge."
	);
	XtkNodeRef modes = xtk_ref_local(ui_row(txn, page_example(b, parent, "Content modes."), 24.0f, true));
	labeled(txn, modes, "Display name", (XtkAvatarProps) {.text = "John Smith"});
	labeled(txn, modes, "Initials", (XtkAvatarProps) {.initials = "AB"});
	labeled(txn, modes, "Placeholder", (XtkAvatarProps) {0});
	labeled(txn, modes, "Group", (XtkAvatarProps) {.group = true});
	XtkNodeRef badges = xtk_ref_local(ui_row(txn, page_example(b, parent, "Badges."), 24.0f, true));
	labeled(txn, badges, "Number", (XtkAvatarProps) {.text = "Ada Lovelace", .badgevalue = 4});
	labeled(txn, badges, "Overflow", (XtkAvatarProps) {.text = "Grace Hopper", .badgevalue = 128});
	labeled(txn, badges, "Glyph", (XtkAvatarProps) {.text = "Alan Turing", .badgeglyph = "Mail"});
	XtkNodeRef card = xtk_ref_local(
	  ui_card(txn, page_example(b, parent, "Live badge + group toggle."), 16.0f, XENT_FLEX_ALIGN_CENTER, 16.0f)
	);
	XtkLocalId live = avatar(
	  txn, card, (XtkAvatarProps) {.text = "Fluxent User", .badgevalue = m->pp_badge, .group = m->pp_group, .w = 96.0f}
	);
	XtkNodeRef row = xtk_ref_local(ui_row(txn, card, 12.0f, true));
	page_button(b, row, "Badge -", MSG_PP_BADGE, -1);
	page_keepout(b, MSG_PP_BADGE, page_note(b, row, gallery_fmt(b->app, "Badge: %d", m->pp_badge)));
	page_button(b, row, "Badge +", MSG_PP_BADGE, 1);
	page_checkbox(b, card, "Group mode", m->pp_group, MSG_PP_GROUP);
	page_keep(b, MSG_PP_BADGE, live);
	return true;
}
