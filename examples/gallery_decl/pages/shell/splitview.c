#include "gallery.h"

static char const *const kSplitModes [] = {"Inline", "CompactInline", "Overlay", "CompactOverlay"};

static XtkEl            *split_pane_item(XtkUi *ui, char const *icon, char const *label) {
	return xtk_button(ui, label, (XtkButtonDesc) {.icon = icon, .style = XTK_BUTTON_SUBTLE});
}

XtkEl *page_split_view(XtkUi *ui, Model const *m) {
	XtkEl *pane = xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 4, .align = XENT_FLEX_ALIGN_STRETCH, .padding = {8, 8, 8, 8}
    },
	  xtk_kids(
		split_pane_item(ui, "Home", "Home"), split_pane_item(ui, "Folder", "Documents"),
		split_pane_item(ui, "Pictures", "Pictures"), split_pane_item(ui, "Settings", "Settings")
	  )
	);

	XtkEl *content = xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 12, .align = XENT_FLEX_ALIGN_START, .padding = {16, 16, 16, 16}
    },
	  xtk_kids(
		xtk_button(ui, "\xE2\x98\xB0 Toggle pane", (XtkButtonDesc) {.click = xtk_msg(MSG_SPLIT_TOGGLE)}),
		xtk_text(ui, "Main content area", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, m->split_open ? "The pane is open." : "The pane is collapsed.", (XtkTextDesc) {.size = 13})
	  )
	);

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "SplitView",
		  "A collapsible side pane beside main content: Inline and CompactInline "
		  "push the content; Overlay and CompactOverlay float the pane over it."
		),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
		  xtk_kids(
			xtk_combo(
			  ui, "Display mode",
			  (XtkComboDesc) {.items = kSplitModes, .nitem = 4, .sel = m->split_mode, .select = xtk_msg(MSG_SPLIT_MODE)}
			),
			xtk_checkbox(
			  ui, "Pane on right", (XtkToggleDesc) {.checked = m->split_right, .change = xtk_msg(MSG_SPLIT_PLACE)}
			)
		  )
		),
		xtk_sized(
		  xtk_splitview(
			ui,
			(XtkSplitViewDesc) {
			  .pane_open      = m->split_open,
			  .display_mode   = m->split_mode,
			  .pane_placement = m->split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT,
			},
			pane, content
		  ),
		  NAN, 320.0f
		)
	  )
	);
}
