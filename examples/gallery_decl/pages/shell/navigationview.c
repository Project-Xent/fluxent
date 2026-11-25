#include "gallery.h"

static XtkEl *nav_demo_header(XtkUi *ui, void const *header) {
	return xtk_margin(
	  xtk_text(ui, header, (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}), (XentInsets) {16, 8, 16, 0}
	);
}

static XtkEl *nav_demo_pane_header(XtkUi *ui) {
	XtkEl *row = xtk_row(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER},
	  xtk_kids(xtk_text(ui, "Menu", (XtkTextDesc) {.size = 14, .nowrap = true}))
	);
	return xtk_nav_slot(ui, XTK_NAV_SLOT_HEADER, xtk_sized(row, NAN, 48.0f));
}

static XtkEl *nav_demo(XtkUi *ui, Model const *m) {
	XtkNavPaneMode mode = kNavDemoPaneModes [m->nav_demo_mode];
	bool           top  = mode == XTK_NAV_PANE_TOP;
	return xtk_sized(
	  xtk_nav(
		ui,
		(XtkNavDesc) {
		  .items          = kNavDemoItems,
		  .nitem          = GALLERY_NAV_DEMO_ITEMS,
		  .sel            = m->nav_demo_sel,
		  .pane_mode      = mode,
		  .pane_title     = "Navigation",
		  .hide_settings  = m->nav_demo_hide,
		  .hide_header    = m->nav_demo_hide_header,
		  .back_enabled   = m->nav_demo_sel != GALLERY_NAV_DEMO_HOME,
		  .back_requested = xtk_msg(MSG_NAV_DEMO),
		  .select         = xtk_msg(MSG_NAV_DEMO),
	},
		xtk_kids(
		  top ? nav_demo_pane_header(ui) : NULL,
		  xtk_nav_slot(
			ui, XTK_NAV_SLOT_SEARCH,
			xtk_suggest(
			  ui, "Find a page",
			  (XtkAutoSuggestDesc) {.suggestions = kNavDemoSuggestions, .nitem = 2, .query_icon = "Search"}
			)
		  ),
		  xtk_nav_header_template(ui, "Page header", nav_demo_header),
		  xtk_nav_slot(
			ui, XTK_NAV_SLOT_CONTENT_OVERLAY,
			xtk_margin(xtk_text(ui, "Navigation preview", (XtkTextDesc) {.size = 12}), (XentInsets) {8, 6, 8, 6})
		  ),
		  xtk_column(
			ui, (XtkStackDesc) {.padding = {16, 12, 16, 12}},
			xtk_kids(xtk_text(
			  ui, xtk_fmt(ui, "Selected: %s", gallery_nav_demo_title(m->nav_demo_sel)), (XtkTextDesc) {.size = 14}
			))
		  )
		)
	  ),
	  NAN, 440.0f
	);
}

static XtkEl *nav_demo_options(XtkUi *ui, Model const *m) {
	return xtk_row(
	  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
	  xtk_kids(
		xtk_combo(
		  ui, "Pane display mode",
		  (XtkComboDesc) {
			.items  = kNavDemoModes,
			.nitem  = GALLERY_NAV_DEMO_MODES,
			.sel    = m->nav_demo_mode,
			.select = xtk_msg(MSG_NAV_DEMO_MODE)}
		),
		xtk_checkbox(
		  ui, "Hide Settings", (XtkToggleDesc) {.checked = m->nav_demo_hide, .change = xtk_msg(MSG_NAV_DEMO_HIDE)}
		),
		xtk_button(ui, "Select Settings", (XtkButtonDesc) {.click = xtk_msg(MSG_NAV_DEMO_PICK)}),
		xtk_checkbox(
		  ui, "Hide page header outside Minimal",
		  (XtkToggleDesc) {.checked = m->nav_demo_hide_header, .change = xtk_msg(MSG_NAV_DEMO_HIDE_HEADER)}
		)
	  )
	);
}

XtkEl *page_navigation_view(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "NavigationView", "Try the pane modes, Back, and the built-in Settings item."),
		nav_demo_options(ui, m), nav_demo(ui, m)
	  )
	);
}
