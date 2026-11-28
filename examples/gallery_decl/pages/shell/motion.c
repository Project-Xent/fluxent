#include "gallery.h"

#include <fluxent/flux_theme.h>

static XtkEl *pill_btn(XtkUi *ui, char const *label, bool active, XtkMsg msg) {
	return xtk_button(
	  ui, label, (XtkButtonDesc) {.style = active ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD, .click = msg}
	);
}

static XtkStyle implicit_style_for(int step) {
	if (step == 1) {
		return (XtkStyle) {
		  .mask          = XTK_STYLE_OPACITY
		                 | XTK_STYLE_TRANSLATE_X
		                 | XTK_STYLE_SCALE_X
		                 | XTK_STYLE_SCALE_Y
		                 | XTK_STYLE_BACKGROUND
		                 | XTK_STYLE_BORDER_COLOR
		                 | XTK_STYLE_CORNER_RADIUS,
		  .opacity       = 0.72f,
		  .translate_x   = 110.0f,
		  .scale_x       = 1.12f,
		  .scale_y       = 1.12f,
		  .background    = xtk_color_rgba(0xc2, 0x39, 0xb3, 0xcc),
		  .border_color  = xtk_color_rgb(0xff, 0x8c, 0x00),
		  .corner_radius = 18.0f,
		};
	}
	if (step == 2) {
		return (XtkStyle) {
		  .mask          = XTK_STYLE_OPACITY
		                 | XTK_STYLE_TRANSLATE_X
		                 | XTK_STYLE_TRANSLATE_Y
		                 | XTK_STYLE_SCALE_X
		                 | XTK_STYLE_SCALE_Y
		                 | XTK_STYLE_ROTATION
		                 | XTK_STYLE_BACKGROUND
		                 | XTK_STYLE_BORDER_COLOR
		                 | XTK_STYLE_CORNER_RADIUS,
		  .opacity       = 0.95f,
		  .translate_x   = 210.0f,
		  .translate_y   = 8.0f,
		  .scale_x       = 0.92f,
		  .scale_y       = 0.92f,
		  .rotation      = 8.0f,
		  .background    = xtk_color_rgba(0x00, 0x82, 0x72, 0xd8),
		  .border_color  = xtk_color_rgb(0x50, 0xe6, 0xff),
		  .corner_radius = 28.0f,
		};
	}
	return (XtkStyle) {
	  .mask          = XTK_STYLE_OPACITY
	                 | XTK_STYLE_TRANSLATE_X
	                 | XTK_STYLE_SCALE_X
	                 | XTK_STYLE_SCALE_Y
	                 | XTK_STYLE_ROTATION
	                 | XTK_STYLE_BACKGROUND
	                 | XTK_STYLE_BORDER_COLOR
	                 | XTK_STYLE_CORNER_RADIUS,
	  .opacity       = 1.0f,
	  .scale_x       = 1.0f,
	  .scale_y       = 1.0f,
	  .background    = xtk_color_rgba(0x00, 0x78, 0xd4, 0xcc),
	  .border_color  = xtk_color_rgb(0x60, 0xcd, 0xff),
	  .corner_radius = 8.0f,
	};
}

static XtkEl *implicit_section(XtkUi *ui, Model const *m) {
	XtkEl *masks = xtk_row(
	  ui, (XtkStackDesc) {.gap = 6, .wrap = XENT_FLEX_WRAP},
	  xtk_kids(
		pill_btn(
		  ui, "Opacity", m->motion_imp_mask & XTK_MOTION_OPACITY, xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_OPACITY)
		),
		pill_btn(
		  ui, "Translate", m->motion_imp_mask & XTK_MOTION_TRANSLATE_X,
		  xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_TRANSLATE)
		),
		pill_btn(
		  ui, "Scale", m->motion_imp_mask & XTK_MOTION_SCALE_X, xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_SCALE)
		),
		pill_btn(
		  ui, "Rotation", m->motion_imp_mask & XTK_MOTION_ROTATION, xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_ROTATION)
		),
		pill_btn(
		  ui, "Brush", m->motion_imp_mask & XTK_MOTION_BACKGROUND,
		  xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_BACKGROUND | XTK_MOTION_BORDER_COLOR)
		),
		pill_btn(
		  ui, "Radius", m->motion_imp_mask & XTK_MOTION_CORNER_RADIUS,
		  xtk_msg_i(MSG_MOTION_IMP_MASK, XTK_MOTION_CORNER_RADIUS)
		)
	  )
	);
	XtkEl *targets = xtk_row(
	  ui, (XtkStackDesc) {.gap = 8},
	  xtk_kids(
		pill_btn(ui, "State A", m->motion_imp_step == 0, xtk_msg_i(MSG_MOTION_IMP_STATE, 0)),
		pill_btn(ui, "State B", m->motion_imp_step == 1, xtk_msg_i(MSG_MOTION_IMP_STATE, 1)),
		pill_btn(ui, "State C", m->motion_imp_step == 2, xtk_msg_i(MSG_MOTION_IMP_STATE, 2))
	  )
	);
	XtkStyle  st   = implicit_style_for(m->motion_imp_step);
	XtkMotion mo   = {.implicit_mask = m->motion_imp_mask};
	XtkEl    *card = xtk_motion(
	  ui,
	  xtk_style(
		ui,
		xtk_sized(
		  xtk_card(
			ui,
			(XtkStackDesc) {
			  .padding = {14, 10, 14, 10},
                .align = XENT_FLEX_ALIGN_CENTER
    },
			xtk_kids(xtk_text(ui, "Implicit Target", (XtkTextDesc) {.size = 13, .weight = FLUX_FONT_SEMI_BOLD}))
		  ),
		  180, 64
		),
		&st
	  ),
	  &mo
	);
	return example(
	  ui, "Implicit Property Transitions (click states rapidly to verify mid-flight retargeting)",
	  xtk_column(ui, (XtkStackDesc) {.gap = 10}, xtk_kids(masks, targets, card))
	);
}

static XtkEl *enter_exit_section(XtkUi *ui, Model const *m) {
	XtkEl *modes = xtk_row(
	  ui, (XtkStackDesc) {.gap = 6, .wrap = XENT_FLEX_WRAP},
	  xtk_kids(
		pill_btn(
		  ui, "Entrance", m->motion_theme_kind == XTK_THEME_TRANSITION_ENTRANCE,
		  xtk_msg_i(MSG_MOTION_ENTER_KIND, XTK_THEME_TRANSITION_ENTRANCE)
		),
		pill_btn(
		  ui, "AddDelete", m->motion_theme_kind == XTK_THEME_TRANSITION_ADD_DELETE,
		  xtk_msg_i(MSG_MOTION_ENTER_KIND, XTK_THEME_TRANSITION_ADD_DELETE)
		),
		pill_btn(
		  ui, "Content", m->motion_theme_kind == XTK_THEME_TRANSITION_CONTENT,
		  xtk_msg_i(MSG_MOTION_ENTER_KIND, XTK_THEME_TRANSITION_CONTENT)
		),
		pill_btn(
		  ui, "Reorder", m->motion_theme_kind == XTK_THEME_TRANSITION_REORDER,
		  xtk_msg_i(MSG_MOTION_ENTER_KIND, XTK_THEME_TRANSITION_REORDER)
		),
		pill_btn(
		  ui, "Popup", m->motion_theme_kind == XTK_THEME_TRANSITION_POPUP,
		  xtk_msg_i(MSG_MOTION_ENTER_KIND, XTK_THEME_TRANSITION_POPUP)
		)
	  )
	);
	XtkEl *actions = xtk_row(
	  ui, (XtkStackDesc) {.gap = 8},
	  xtk_kids(
		xtk_button(ui, "Add item", (XtkButtonDesc) {.click = xtk_msg(MSG_MOTION_ITEM_ADD)}),
		xtk_button(ui, "Remove last", (XtkButtonDesc) {.click = xtk_msg(MSG_MOTION_ITEM_REMOVE)}),
		xtk_button(ui, "Shift order", (XtkButtonDesc) {.click = xtk_msg(MSG_MOTION_ITEM_SHUFFLE)})
	  )
	);
	XtkEl   **items = xtk_children_alloc(ui, m->motion_item_count);
	XtkMotion mo    = {.enter_exit = ( XtkThemeTransitionKind ) m->motion_theme_kind};
	for (int i = 0; i < m->motion_item_count; i++) {
		int    id   = m->motion_item_ids [i];
		XtkEl *card = xtk_card(
		  ui,
		  (XtkStackDesc) {
			.padding = {12, 8, 12, 8}
        },
		  xtk_kids(xtk_text(ui, xtk_fmt(ui, "Item #%d", id), (XtkTextDesc) {.size = 13}))
		);
		items [i] = xtk_keyed(ui, xtk_fmt(ui, "motion-item-%d", id), xtk_motion(ui, card, &mo));
	}
	return example(
	  ui, "Enter, Exit & Reorder Theme Transitions (with PVL stagger & unloading ghost storage)",
	  xtk_column(
		ui, (XtkStackDesc) {.gap = 10},
		xtk_kids(modes, actions, xtk_row(ui, (XtkStackDesc) {.gap = 8, .wrap = XENT_FLEX_WRAP}, items))
	  )
	);
}

static XtkEl *frame_section(XtkUi *ui, Model const *m) {
	static char const *const kTitles [] = {"Alpha", "Beta", "Gamma", "Delta"};
	XtkEl                   *kinds      = xtk_row(
	  ui, (XtkStackDesc) {.gap = 6, .wrap = XENT_FLEX_WRAP},
	  xtk_kids(
		pill_btn(
		  ui, "Entrance", m->motion_frame_kind == XTK_NAV_TRANSITION_ENTRANCE,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_ENTRANCE)
		),
		pill_btn(
		  ui, "SlideRight", m->motion_frame_kind == XTK_NAV_TRANSITION_SLIDE_RIGHT,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_SLIDE_RIGHT)
		),
		pill_btn(
		  ui, "SlideLeft", m->motion_frame_kind == XTK_NAV_TRANSITION_SLIDE_LEFT,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_SLIDE_LEFT)
		),
		pill_btn(
		  ui, "SlideBottom", m->motion_frame_kind == XTK_NAV_TRANSITION_SLIDE_BOTTOM,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_SLIDE_BOTTOM)
		),
		pill_btn(
		  ui, "DrillIn", m->motion_frame_kind == XTK_NAV_TRANSITION_DRILL_IN,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_DRILL_IN)
		),
		pill_btn(
		  ui, "Suppress", m->motion_frame_kind == XTK_NAV_TRANSITION_SUPPRESS,
		  xtk_msg_i(MSG_MOTION_FRAME_KIND, XTK_NAV_TRANSITION_SUPPRESS)
		)
	  )
	);
	XtkEl *nav = xtk_row(
	  ui, (XtkStackDesc) {.gap = 8},
	  xtk_kids(
		xtk_button(ui, "Back", (XtkButtonDesc) {.click = xtk_msg_i(MSG_MOTION_FRAME_STEP, -1)}),
		xtk_button(
		  ui, "Next page", (XtkButtonDesc) {.style = XTK_BUTTON_ACCENT, .click = xtk_msg_i(MSG_MOTION_FRAME_STEP, 1)}
		)
	  )
	);
	int    idx     = m->motion_frame_page & 3;
	XtkEl *subpage = xtk_keyed(
	  ui, xtk_fmt(ui, "sub-%d", idx),
	  xtk_card(
		ui,
		(XtkStackDesc) {
		  .gap = 4, .padding = {16, 14, 16, 14}
    },
		xtk_kids(
		  xtk_text(
			ui, xtk_fmt(ui, "Subpage %s", kTitles [idx]), (XtkTextDesc) {.size = 16, .weight = FLUX_FONT_SEMI_BOLD}
		  ),
		  xtk_text(ui, m->motion_frame_back ? "Direction: Back" : "Direction: Forward", (XtkTextDesc) {.size = 12})
		)
	  )
	);
	XtkEl *frame = xtk_sized(
	  xtk_frame(
		ui,
		(XtkFrameDesc) {
		  .transition = ( XtkNavTransitionKind ) m->motion_frame_kind,
		  .is_back    = m->motion_frame_back,
		  .page_key   = idx + 1,
		},
		subpage
	  ),
	  480, 90
	);
	return example(
	  ui, "Frame NavigationTransitionInfo (all 6 WinUI transition kinds & Back direction)",
	  xtk_column(ui, (XtkStackDesc) {.gap = 10}, xtk_kids(kinds, nav, frame))
	);
}

static XtkEl *connected_section(XtkUi *ui, Model const *m) {
	XtkEl *modes = xtk_row(
	  ui, (XtkStackDesc) {.gap = 6},
	  xtk_kids(
		pill_btn(
		  ui, "Gravity", m->motion_conn_kind == XTK_CONNECTED_GRAVITY,
		  xtk_msg_i(MSG_MOTION_CONN_KIND, XTK_CONNECTED_GRAVITY)
		),
		pill_btn(
		  ui, "Direct", m->motion_conn_kind == XTK_CONNECTED_DIRECT,
		  xtk_msg_i(MSG_MOTION_CONN_KIND, XTK_CONNECTED_DIRECT)
		),
		pill_btn(
		  ui, "Basic", m->motion_conn_kind == XTK_CONNECTED_BASIC, xtk_msg_i(MSG_MOTION_CONN_KIND, XTK_CONNECTED_BASIC)
		),
		xtk_button(
		  ui, m->motion_conn_detail ? "Collapse" : "Expand detail",
		  (XtkButtonDesc) {.style = XTK_BUTTON_ACCENT, .click = xtk_msg(MSG_MOTION_CONN_TOGGLE)}
		)
	  )
	);
	XtkConnectedDesc cfg     = {.kind = ( XtkConnectedKind ) m->motion_conn_kind};
	XtkStyle         hero_st = {
	  .mask          = XTK_STYLE_BACKGROUND | XTK_STYLE_CORNER_RADIUS,
	  .background    = xtk_color_rgba(0x00, 0x78, 0xd4, 0xe0),
	  .corner_radius = 10.0f,
	};
	XtkEl *hero = xtk_connected(
	  ui,
	  xtk_style(
		ui,
		xtk_sized(
		  xtk_card(
			ui,
			(XtkStackDesc) {
			  .padding = {12, 10, 12, 10}
    },
			xtk_kids(xtk_text(ui, "Hero", (XtkTextDesc) {.size = 13, .weight = FLUX_FONT_SEMI_BOLD}))
		  ),
		  m->motion_conn_detail ? 200 : 96, m->motion_conn_detail ? 100 : 64
		),
		&hero_st
	  ),
	  "motion-hero", &cfg
	);
	XtkEl *content
	  = m->motion_conn_detail
	    ? xtk_keyed(
			ui, "conn-detail",
			xtk_row(
			  ui,
			  (XtkStackDesc) {
				.gap = 24, .padding = {0, 18, 0, 18},
                     .align = XENT_FLEX_ALIGN_CENTER
    },
			  xtk_kids(
				xtk_connected_coord(
				  ui,
				  xtk_sized(
					xtk_column(
					  ui, (XtkStackDesc) {.gap = 4},
					  xtk_kids(
						xtk_text(ui, "Coordinated Details", (XtkTextDesc) {.size = 15, .weight = FLUX_FONT_SEMI_BOLD}),
						xtk_text(ui, "Flies along alongside the hero element", (XtkTextDesc) {.size = 12})
					  )
					),
					220, NAN
				  ),
				  "motion-hero"
				),
				hero
			  )
			)
		  )
	    : xtk_keyed(
			ui, "conn-thumb",
			xtk_row(
			  ui, (XtkStackDesc) {.gap = 16, .padding = {0, 18, 0, 18}, .align = XENT_FLEX_ALIGN_CENTER},
			  xtk_kids(
				hero,
				xtk_text(ui, "Thumbnail state - select a mode and click Expand detail", (XtkTextDesc) {.size = 12})
			  )
			)
		  );
	return example(
	  ui, "Connected Animation (Gravity dip & shadow, Direct, Basic + coordinated elements)",
	  xtk_column(ui, (XtkStackDesc) {.gap = 10}, xtk_kids(xtk_keyed(ui, "conn-modes", modes), content))
	);
}

static XtkEl *reduced_motion_section(XtkUi *ui, Model const *m) {
	bool enabled = flux_animations_enabled();
	return example(
	  ui, "Reduced Motion (UISettings.AnimationsEnabled)",
	  xtk_row(
		ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
		xtk_kids(
		  pill_btn(ui, "System", m->motion_reduced_override < 0, xtk_msg_i(MSG_MOTION_REDUCED, -1)),
		  pill_btn(ui, "Force Off", m->motion_reduced_override == 0, xtk_msg_i(MSG_MOTION_REDUCED, 0)),
		  pill_btn(ui, "Force On", m->motion_reduced_override == 1, xtk_msg_i(MSG_MOTION_REDUCED, 1)),
		  output_col(ui, enabled ? "Active: Enabled" : "Active: Reduced Motion (Disabled)")
		)
	  )
	);
}

XtkEl *page_motion(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Motion",
		  "Implicit property transitions, enter/exit/reorder theme transitions, Frame NavigationTransitionInfo, "
		  "ConnectedAnimation, and UISettings.AnimationsEnabled."
		),
		implicit_section(ui, m), enter_exit_section(ui, m), frame_section(ui, m), connected_section(ui, m),
		reduced_motion_section(ui, m)
	  )
	);
}
