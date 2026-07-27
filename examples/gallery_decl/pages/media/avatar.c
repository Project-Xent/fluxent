#include "gallery.h"

static XtkEl *avatar(XtkUi *ui, XtkPersonPictureDesc desc) {
	if (desc.size <= 0.0f) desc.size = 72.0f;
	return xtk_avatar(ui, desc);
}

static XtkEl *avatar_labeled(XtkUi *ui, char const *caption, XtkPersonPictureDesc desc) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
	  xtk_kids(avatar(ui, desc), xtk_text(ui, caption, (XtkTextDesc) {.size = 12}))
	);
}

XtkEl *page_person_picture(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "PersonPicture",
		  "A circular avatar that shows a profile photo, initials derived from a "
		  "display name, or a placeholder glyph — with an optional badge."
		),
		example(
		  ui, "Content modes.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_START, .wrap = XENT_FLEX_WRAP},
			xtk_kids(
			  avatar_labeled(ui, "Display name", (XtkPersonPictureDesc) {.display_name = "John Smith"}),
			  avatar_labeled(ui, "Initials", (XtkPersonPictureDesc) {.initials = "AB"}),
			  avatar_labeled(ui, "Placeholder", (XtkPersonPictureDesc) {0}),
			  avatar_labeled(ui, "Group", (XtkPersonPictureDesc) {.is_group = true})
			)
		  )
		),
		example(
		  ui, "Badges.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_START, .wrap = XENT_FLEX_WRAP},
			xtk_kids(
			  avatar_labeled(ui, "Number", (XtkPersonPictureDesc) {.display_name = "Ada Lovelace", .badge_number = 4}),
			  avatar_labeled(
				ui, "Overflow", (XtkPersonPictureDesc) {.display_name = "Grace Hopper", .badge_number = 128}
			  ),
			  avatar_labeled(ui, "Glyph", (XtkPersonPictureDesc) {.display_name = "Alan Turing", .badge_glyph = "Mail"})
			)
		  )
		),
		example(
		  ui, "Live badge + group toggle.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER},
			xtk_kids(
			  avatar(
				ui,
				(XtkPersonPictureDesc) {
				  .display_name = "Fluxent User",
				  .badge_number = m->pp_badge,
				  .is_group     = m->pp_group,
				  .size         = 96.0f,
				}
			  ),
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
				xtk_kids(
				  xtk_button(ui, "Badge -", (XtkButtonDesc) {.click = xtk_msg_i(MSG_PP_BADGE, -1)}),
				  xtk_text(ui, xtk_fmt(ui, "Badge: %d", m->pp_badge), (XtkTextDesc) {.size = 13}),
				  xtk_button(ui, "Badge +", (XtkButtonDesc) {.click = xtk_msg_i(MSG_PP_BADGE, +1)})
				)
			  ),
			  xtk_checkbox(ui, "Group mode", (XtkToggleDesc) {.checked = m->pp_group, .change = xtk_msg(MSG_PP_GROUP)})
			)
		  )
		)
	  )
	);
}
