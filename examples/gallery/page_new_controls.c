/**
 * @file page_new_controls.c
 * @brief Gallery demos for the nine WinUI 3 controls added in fluxent 0.1:
 * RadioButtons, RatingControl, ToggleSplitButton, BreadcrumbBar, SelectorBar,
 * TeachingTip, TreeView, ItemsView, and PullToRefresh.
 */
#include "gallery.h"

/* ---------------------------------------------------------------- RadioButtons */

static XtkRadioItem const kRbItems [] = {
  {.label = "Option A"},
  {.label = "Option B"},
  {.label = "Option C"},
};

XtkEl *page_radio_buttons(XtkUi *ui, Model const *m) {
	char const *status = m->radio_buttons_sel >= 0 && m->radio_buttons_sel < 3
	                       ? kRbItems [m->radio_buttons_sel].label
	                       : "Nothing selected";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "RadioButtons",
		  "A grouped set of mutually exclusive radio options — one control, one selection index."
		),
		example(
		  ui, "A RadioButtons group with a header.",
		  demo_card(
			ui,
			xtk_radio_buttons(
			  ui, (XtkRadioButtonsDesc) {
				.header    = "Background",
				.items     = kRbItems,
				.item_count = 3,
				.selected  = m->radio_buttons_sel,
				.on_select = xtk_msg(MSG_RADIO_BUTTONS),
			      }
			),
			xtk_fmt(ui, "Selected: %s", status), NULL
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- RatingControl */

XtkEl *page_rating(XtkUi *ui, Model const *m) {
	char const *out = m->rating_value >= 0.0 ? xtk_fmt(ui, "Value: %.0f", m->rating_value)
	                                         : "Unset (-1). Click a star to rate.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "RatingControl",
		  "A star strip: hovering previews the prospective rating under the pointer, a click commits, "
		  "and re-clicking the current rating clears it back to unset (-1)."
		),
		example(
		  ui, "An interactive RatingControl with a caption.",
		  demo_card(
			ui,
			xtk_rating(
			  ui, (XtkRatingDesc) {
				.value            = m->rating_value,
				.is_clear_enabled = true,
				.caption          = "Your rating",
				.on_change        = xtk_msg(MSG_RATING),
			      }
			),
			out, NULL
		  )
		),
		example(
		  ui, "Read-only, displaying a fractional average as a partial fill.",
		  demo_card(
			ui,
			xtk_rating(
			  ui, (XtkRatingDesc) {.value = 3.5, .is_read_only = true, .caption = "154 ratings"}
			),
			"Value: 3.5", NULL
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- ToggleSplitButton */

XtkEl *page_toggle_split(XtkUi *ui, Model const *m) {
	static char const *const kMenuOut [] = {"(primary click)", "Align left", "Align center", "Align right"};
	int menu = m->toggle_split_menu >= 0 && m->toggle_split_menu <= 3 ? m->toggle_split_menu : 0;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "ToggleSplitButton",
		  "The left zone toggles checked state; the chevron opens a menu of secondary actions."
		),
		example(
		  ui, "Toggle + menu split button.",
		  demo_card(
			ui,
			xtk_toggle_split_button(
			  ui, "Bold",
			  (XtkSplitDesc) {
				.icon      = "Bold",
				.checked   = m->toggle_split_checked,
				.on_toggle = xtk_msg(MSG_TOGGLE_SPLIT),
				.on_click  = xtk_msg(MSG_TOGGLE_SPLIT_CLICK),
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Align left", .on_click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 0)},
					{.label = "Align center", .on_click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 1)},
					{.label = "Align right", .on_click = xtk_msg_i(MSG_TOGGLE_SPLIT_MENU, 2)},
				  },
				.item_count = 3,
			      }
			),
			xtk_fmt(ui, "%s — %s", m->toggle_split_checked ? "On" : "Off", kMenuOut [menu]), NULL
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- BreadcrumbBar */

static char const *const kCrumbs [] = {"Home", "Documents", "Projects", "FluXent"};

XtkEl *page_breadcrumb(XtkUi *ui, Model const *m) {
	char const *status = m->breadcrumb_click >= 0 && m->breadcrumb_click < 4
	                       ? xtk_fmt(ui, "Clicked: %s", kCrumbs [m->breadcrumb_click])
	                       : "Click a crumb to navigate.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "BreadcrumbBar",
		  "Shows the path to the current location. Every crumb except the last posts on_click."
		),
		example(
		  ui, "A four-segment breadcrumb.",
		  demo_card(
			ui,
			xtk_breadcrumb_bar(
			  ui, (XtkBreadcrumbDesc) {
				.items    = kCrumbs,
				.count    = 4,
				.on_click = xtk_msg(MSG_BREADCRUMB),
			      }
			),
			status, NULL
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- SelectorBar */

static XtkSelectorItem const kSelectors [] = {
  {.text = "All", .icon = "List"},
  {.text = "Music", .icon = "MusicNote"},
  {.text = "Video", .icon = "Video"},
  {.text = "Photos", .icon = "Pictures"},
};

XtkEl *page_selector_bar(XtkUi *ui, Model const *m) {
	char const *status = m->selector_sel >= 0 && m->selector_sel < 4
	                       ? xtk_fmt(ui, "Filter: %s", kSelectors [m->selector_sel].text)
	                       : "No filter selected.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "SelectorBar",
		  "A horizontal strip of pill toggles — one selected item at a time."
		),
		example(
		  ui, "Icon+text selector items.",
		  demo_card(
			ui,
			xtk_selector_bar(
			  ui, (XtkSelectorBarDesc) {
				.items      = kSelectors,
				.item_count = 4,
				.selected   = m->selector_sel,
				.on_select  = xtk_msg(MSG_SELECTOR_BAR),
			      }
			),
			status, NULL
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- TeachingTip */

XtkEl *page_teaching_tip(XtkUi *ui, Model const *m) {
	char const *status = m->tip_open ? "Tip is open." : "Tip is closed.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "TeachingTip",
		  "A lightweight teaching surface with subtitle, action, and close affordances."
		),
		example(
		  ui, "Untargeted tip toggled from the model.",
		  demo_card(
			ui,
			xtk_button(ui, m->tip_open ? "Hide tip" : "Show tip", (XtkButtonDesc) {.on_click = xtk_msg(MSG_TIP_OPEN)}),
			status, NULL
		  )
		),
		xtk_teaching_tip(
		  ui, "Welcome to FluXent",
		  (XtkTeachingTipDesc) {
			.open          = m->tip_open,
			.subtitle      = "Explore the WinUI 3 control gallery.",
			.icon          = "Lightbulb",
			.action_text   = "Got it",
			.action_accent = true,
			.untargeted    = true,
			.on_action     = xtk_msg(MSG_TIP_ACTION),
			.on_close      = xtk_msg(MSG_TIP_CLOSE),
		  },
		  (XtkEl *[]) {XTK_END}
		),
		example(
		  ui, "Tip events.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			(XtkEl *[]) {
			  xtk_text(
				ui,
				xtk_fmt(
				  ui, "Action clicks: %d. Last close reason: %d.", m->tip_actions,
				  m->tip_close_reason
				),
				(XtkTextDesc) {.size = 13}
			  ),
			  XTK_END}
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- TreeView */

static XtkTreeNodeDesc const kTreeDocsChildren [] = {
  {.text = "Proposal.docx"},
  {.text = "Budget.xlsx"},
};

static XtkTreeNodeDesc const kTreePicturesChildren [] = {
  {.text = "Vacation"},
  {.text = "Screenshots"},
};

static XtkTreeNodeDesc const kTreeRoots [] = {
  {
	.text        = "Documents",
	.icon        = "Folder",
	.expanded    = true,
	.children    = kTreeDocsChildren,
	.child_count = 2,
  },
  {
	.text        = "Pictures",
	.icon        = "Pictures",
	.children    = kTreePicturesChildren,
	.child_count = 2,
  },
  {.text = "Music", .icon = "MusicNote"},
};

XtkEl *page_tree_view(XtkUi *ui, Model const *m) {
	char const *sel = m->tree_sel >= 0 ? xtk_fmt(ui, "Selected flat row: %d", m->tree_sel) : "Nothing selected.";
	char const *inv = m->tree_invoke >= 0 ? xtk_fmt(ui, "Last invoked: %d", m->tree_invoke) : "Double-click a row to invoke.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "TreeView",
		  "Hierarchical data flattened into a virtualized list. Chevron toggles expand; row selects and invokes."
		),
		example(
		  ui, "Sample folder tree.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			(XtkEl *[]) {
			  xtk_sized(
				xtk_tree_view(
				  ui, (XtkTreeDesc) {
					.roots      = kTreeRoots,
					.root_count = 3,
					.row_height = 28.0f,
					.on_select  = xtk_msg(MSG_TREE_SELECT),
					.on_invoke  = xtk_msg(MSG_TREE_INVOKE),
					.on_expand  = xtk_msg(MSG_TREE_EXPAND),
				      }
				),
				NAN, 280.0f
			  ),
			  xtk_text(ui, sel, (XtkTextDesc) {.size = 13}),
			  xtk_text(ui, inv, (XtkTextDesc) {.size = 13}), XTK_END}
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- ItemsView */

static XtkEl *items_cell(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_card(
	  ui, (XtkStackDesc) {.padding = {12, 12, 12, 12}, .align = XENT_FLEX_ALIGN_CENTER},
	  (XtkEl *[]) {
		xtk_text(ui, xtk_fmt(ui, "Item %d", index), (XtkTextDesc) {.weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, "Tap to invoke", (XtkTextDesc) {.size = 11}), XTK_END}
	);
}

XtkEl *page_items_view(XtkUi *ui, Model const *m) {
	char const *inv = m->items_invoke >= 0 ? xtk_fmt(ui, "Invoked: %d", m->items_invoke) : "Tap a cell to invoke.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "ItemsView",
		  "ItemsRepeater + layout + ItemContainer chrome. Here: a wrapping flow of 24 cards."
		),
		example(
		  ui, "Flow layout with ItemInvoked (no selection chrome).",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			(XtkEl *[]) {
			  xtk_sized(
				xtk_items_view(
				  ui, (XtkItemsViewDesc) {
					.count                = 24,
					.item_height          = 96.0f,
					.item_width           = 140.0f,
					.item_spacing         = 8.0f,
					.layout               = XTK_ITEMS_LAYOUT_FLOW,
					.item                 = items_cell,
					.selection_mode       = XTK_LIST_SELECT_NONE,
					.item_invoked_enabled = true,
					.on_invoke            = xtk_msg(MSG_ITEMS_INVOKE),
				      }
				),
				NAN, 360.0f
			  ),
			  xtk_text(ui, inv, (XtkTextDesc) {.size = 13}), XTK_END}
		  )
		),
		XTK_END}
	);
}

/* ---------------------------------------------------------------- PullToRefresh */

static XtkEl *refresh_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "Row %d", index), (XtkTextDesc) {0});
}

XtkEl *page_pull_refresh(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "PullToRefresh",
		  "A RefreshContainer wrapping scrollable content. Pull from the top edge to refresh."
		),
		example(
		  ui, "List inside a top-edge pull container.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			(XtkEl *[]) {
			  xtk_sized(
				xtk_refresh_container(
				  ui, (XtkRefreshDesc) {
					.is_refreshing = m->refresh_busy,
					.on_refresh    = xtk_msg(MSG_REFRESH),
				      },
				  (XtkEl *[]) {
					xtk_list_view(
					  ui, (XtkListDesc) {
					        .count       = 40,
					        .item_height = 40.0f,
					        .item        = refresh_row,
					      }
					),
					XTK_END}
				),
				NAN, 360.0f
			  ),
			  xtk_text(
				ui, xtk_fmt(ui, "Refresh count: %d. %s", m->refresh_count, m->refresh_busy ? "Refreshing…" : "Idle"),
				(XtkTextDesc) {.size = 13}
			  ),
			  xtk_button(
				ui, "End refresh",
				(XtkButtonDesc) {.on_click = xtk_msg(MSG_REFRESH_DONE), .disabled = !m->refresh_busy}
			  ),
			  XTK_END}
		  )
		),
		XTK_END}
	);
}
