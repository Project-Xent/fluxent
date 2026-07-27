#include "app.h"
#include <math.h>
#include <stdio.h>

static XentNodeId avatar(GalleryApp *g, XentNodeId parent, FluxPersonPictureDesc desc) {
	if (desc.diameter <= 0.0f) desc.diameter = 72.0f;
	desc.ctx    = gallery_ctx(g);
	desc.store  = gallery_store(g);
	desc.parent = parent;
	return flux_avatar_init(&desc);
}

static XentNodeId avatar_labeled(GalleryApp *g, XentNodeId parent, char const *caption, FluxPersonPictureDesc desc) {
	XentNodeId col = g_col(g, parent, 8.0f, 0, 0, 0, 0);
	xent_setitems(gallery_ctx(g), col, XENT_FLEX_ALIGN_CENTER);
	avatar(g, col, desc);
	g_text(g, col, caption, 12.0f, FLUX_FONT_REGULAR);
	return col;
}

void page_person_picture(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_person_picture_sync);
	Model const *m = &g->model;
	g_page_header(
	  g, parent, "PersonPicture",
	  "A circular avatar that shows a profile photo, initials derived from a display name, or a "
	  "placeholder glyph — with an optional badge."
	);
	{
		XentNodeId ex  = g_example(g, parent, "Content modes.");
		XentNodeId row = g_wraprow(g, ex, 24.0f);
		avatar_labeled(g, row, "Display name", (FluxPersonPictureDesc) {.display_name = "John Smith"});
		avatar_labeled(g, row, "Initials", (FluxPersonPictureDesc) {.initials = "AB"});
		avatar_labeled(g, row, "Placeholder", (FluxPersonPictureDesc) {0});
		avatar_labeled(g, row, "Group", (FluxPersonPictureDesc) {.is_group = true});
	}
	{
		XentNodeId ex  = g_example(g, parent, "Badges.");
		XentNodeId row = g_wraprow(g, ex, 24.0f);
		avatar_labeled(g, row, "Number", (FluxPersonPictureDesc) {.display_name = "Ada Lovelace", .badge_number = 4});
		avatar_labeled(
		  g, row, "Overflow", (FluxPersonPictureDesc) {.display_name = "Grace Hopper", .badge_number = 128}
		);
		avatar_labeled(g, row, "Glyph", (FluxPersonPictureDesc) {.display_name = "Alan Turing", .badge_glyph = "Mail"});
	}
	{
		XentNodeId ex   = g_example(g, parent, "Live badge + group toggle.");
		XentNodeId card = g_card(g, ex);
		xent_setitems(gallery_ctx(g), card, XENT_FLEX_ALIGN_CENTER);
		XentNodeId live_avatar = avatar(
		  g, card,
		  (FluxPersonPictureDesc) {
			.display_name = "Fluxent User",
			.badge_number = m->pp_badge,
			.is_group     = m->pp_group,
			.diameter     = 96.0f,
		  }
		);
		XentNodeId ctrl = g_wraprow(g, card, 12.0f);
		XentNodeId dec  = g_button(g, ctrl, "Badge -", NULL, 0, true);
		g_on_invoke_i(g, dec, MSG_PP_BADGE, -1);
		g->live_output [MSG_PP_BADGE]
		  = g_text(g, ctrl, gallery_fmt(g, "Badge: %d", m->pp_badge), 13.0f, FLUX_FONT_REGULAR);
		XentNodeId inc = g_button(g, ctrl, "Badge +", NULL, 0, true);
		g_on_invoke_i(g, inc, MSG_PP_BADGE, 1);
		XentNodeId grp = g_checkbox(g, card, "Group mode", m->pp_group, true);
		g_on_change_bool(g, grp, MSG_PP_GROUP);
		g->live_control [MSG_PP_BADGE] = live_avatar;
	}
}

void page_person_picture_sync(GalleryApp *g) {
	XentNodeId avatar = g->live_control [MSG_PP_BADGE];
	flux_avatar_set_badge(gallery_store(g), avatar, NULL, g->model.pp_badge);
	flux_avatar_set_group(gallery_store(g), avatar, g->model.pp_group);
	xent_settext(gallery_ctx(g), g->live_output [MSG_PP_BADGE], gallery_fmt(g, "Badge: %d", g->model.pp_badge));
}
