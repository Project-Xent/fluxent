/**
 * @file page_shell_controls.c
 * @brief Gallery demos for the shell/scaffold controls added after 0.1:
 * PersonPicture, PagerControl, SplitView, and TitleBar.
 */
#include "gallery.h"

/* ---------------------------------------------------------------- PersonPicture */

static XtkEl *avatar(XtkUi *ui, XtkPersonPictureDesc desc) {
	if (desc.size <= 0.0f) desc.size = 72.0f;
	return xtk_person_picture(ui, desc);
}

static XtkEl *avatar_labeled(XtkUi *ui, char const *caption, XtkPersonPictureDesc desc) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
	  (XtkEl *[]) {avatar(ui, desc), xtk_text(ui, caption, (XtkTextDesc) {.size = 12}), XTK_END}
	);
}

XtkEl *page_person_picture(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "PersonPicture",
		  "A circular avatar that shows a profile photo, initials derived from a display name, or a "
		  "placeholder glyph — with an optional badge."
		),
		example(
		  ui, "Content modes.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_START},
			(XtkEl *[]) {
			  avatar_labeled(ui, "Display name", (XtkPersonPictureDesc) {.display_name = "John Smith"}),
			  avatar_labeled(ui, "Initials", (XtkPersonPictureDesc) {.initials = "AB"}),
			  avatar_labeled(ui, "Placeholder", (XtkPersonPictureDesc) {0}),
			  avatar_labeled(ui, "Group", (XtkPersonPictureDesc) {.is_group = true}),
			  XTK_END}
		  )
		),
		example(
		  ui, "Badges.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 24, .align = XENT_FLEX_ALIGN_START},
			(XtkEl *[]) {
			  avatar_labeled(ui, "Number", (XtkPersonPictureDesc) {.display_name = "Ada Lovelace", .badge_number = 4}),
			  avatar_labeled(ui, "Overflow", (XtkPersonPictureDesc) {.display_name = "Grace Hopper", .badge_number = 128}),
			  avatar_labeled(ui, "Glyph", (XtkPersonPictureDesc) {.display_name = "Alan Turing", .badge_glyph = "Mail"}),
			  XTK_END}
		  )
		),
		example(
		  ui, "Live badge + group toggle.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER},
			(XtkEl *[]) {
			  avatar(
				ui, (XtkPersonPictureDesc) {
				  .display_name = "Fluxent User",
				  .badge_number = m->pp_badge,
				  .is_group     = m->pp_group,
				  .size         = 96.0f,
			      }
			  ),
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
				(XtkEl *[]) {
				  xtk_button(ui, "Badge -", (XtkButtonDesc) {.on_click = xtk_msg_i(MSG_PP_BADGE, -1)}),
				  xtk_text(ui, xtk_fmt(ui, "Badge: %d", m->pp_badge), (XtkTextDesc) {.size = 13}),
				  xtk_button(ui, "Badge +", (XtkButtonDesc) {.on_click = xtk_msg_i(MSG_PP_BADGE, +1)}),
				  XTK_END}
			  ),
			  xtk_checkbox(ui, "Group mode", (XtkToggleDesc) {.checked = m->pp_group, .on_change = xtk_msg(MSG_PP_GROUP)}),
			  XTK_END}
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- SplitView */

static char const *const kSplitModes [] = {"Inline", "CompactInline", "Overlay", "CompactOverlay"};

static XtkEl *split_pane_item(XtkUi *ui, char const *icon, char const *label) {
	return xtk_button(ui, label, (XtkButtonDesc) {.icon = icon, .style = XTK_BUTTON_SUBTLE});
}

XtkEl *page_split_view(XtkUi *ui, Model const *m) {
	XtkEl *pane = xtk_column(
	  ui, (XtkStackDesc) {.gap = 4, .align = XENT_FLEX_ALIGN_STRETCH, .padding = {8, 8, 8, 8}},
	  (XtkEl *[]) {
		split_pane_item(ui, "Home", "Home"),
		split_pane_item(ui, "Folder", "Documents"),
		split_pane_item(ui, "Pictures", "Pictures"),
		split_pane_item(ui, "Settings", "Settings"),
		XTK_END}
	);

	XtkEl *content = xtk_column(
	  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_START, .padding = {16, 16, 16, 16}},
	  (XtkEl *[]) {
		xtk_button(ui, "\xE2\x98\xB0 Toggle pane", (XtkButtonDesc) {.on_click = xtk_msg(MSG_SPLIT_TOGGLE)}),
		xtk_text(ui, "Main content area", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(
		  ui, m->split_open ? "The pane is open." : "The pane is collapsed.", (XtkTextDesc) {.size = 13}
		),
		XTK_END}
	);

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "SplitView",
		  "A collapsible side pane beside main content: Inline and CompactInline push the content; Overlay and "
		  "CompactOverlay float the pane over it."
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
		  (XtkEl *[]) {
			xtk_combo_box(
			  ui, "Display mode",
			  (XtkComboDesc) {.items = kSplitModes, .count = 4, .selected = m->split_mode, .on_select = xtk_msg(MSG_SPLIT_MODE)}
			),
			xtk_checkbox(ui, "Pane on right", (XtkToggleDesc) {.checked = m->split_right, .on_change = xtk_msg(MSG_SPLIT_PLACE)}),
			XTK_END}
		),
		xtk_sized(
		  xtk_split_view(
			ui, (XtkSplitViewDesc) {
			  .pane_open      = m->split_open,
			  .display_mode   = m->split_mode,
			  .pane_placement = m->split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT,
		      },
			pane, content
		  ),
		  NAN, 320.0f
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- TitleBar */

XtkEl *page_title_bar(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "TitleBar",
		  "A window title-bar band: an app icon, title and subtitle, plus optional back and pane-toggle "
		  "buttons. Opt into window integration to make the band drag the window."
		),
		example(
		  ui, "Icon, title, subtitle, back and pane-toggle buttons.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 0, .align = XENT_FLEX_ALIGN_STRETCH, .padding = {0, 0, 0, 0}},
			(XtkEl *[]) {
			  xtk_title_bar(
				ui, "FluXent Gallery",
				(XtkTitleBarDesc) {
				  .subtitle         = "Preview",
				  .icon             = "Home",
				  .show_back        = true,
				  .show_pane_toggle = true,
				  .on_back          = xtk_msg(MSG_TB_BACK),
				  .on_pane_toggle   = xtk_msg(MSG_TB_PANE),
			      }
			  ),
			  XTK_END}
		  )
		),
		example(
		  ui, "Button events.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			(XtkEl *[]) {
			  xtk_text(
				ui, xtk_fmt(ui, "Back clicks: %d.  Pane-toggle clicks: %d.", m->tb_back_clicks, m->tb_pane_clicks),
				(XtkTextDesc) {.size = 13}
			  ),
			  XTK_END}
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- PagerControl */

XtkEl *page_pager(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "PagerControl",
		  "A numeric/arrow pager (distinct from the dot PipsPager): First/Previous/Next/Last buttons around a "
		  "number panel with ellipses, or a compact page-of-total label."
		),
		example(
		  ui, "Number panel (32 pages) with First/Last buttons.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_START},
			(XtkEl *[]) {
			  xtk_pager(
				ui, (XtkPagerDesc) {
				  .count        = 32,
				  .selected     = m->pager_page,
				  .first_button = XTK_PAGER_BUTTON_AUTO,
				  .last_button  = XTK_PAGER_BUTTON_AUTO,
				  .on_select    = xtk_msg(MSG_PAGER_SELECT),
			      }
			  ),
			  xtk_text(ui, xtk_fmt(ui, "Page %d of 32", m->pager_page + 1), (XtkTextDesc) {.size = 13}),
			  XTK_END}
		  )
		),
		example(
		  ui, "Number-text display.",
		  xtk_pager(
			ui, (XtkPagerDesc) {
			  .count        = 32,
			  .selected     = m->pager_page,
			  .display_mode = XTK_PAGER_DISPLAY_NUMBER_TEXT,
			  .prefix       = "Page",
			  .suffix       = "of",
			  .on_select    = xtk_msg(MSG_PAGER_SELECT),
		      }
		  )
		),
		XTK_END}
	);
}
