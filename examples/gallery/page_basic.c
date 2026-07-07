#include "gallery.h"

XtkEl *page_button(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Button",
		  "The Button control provides a Click event to respond to user input. It posts an XtkMsg; nothing calls " "back" " int" "o " "your" " cod" "e."
		),
		example(
		  ui, "A simple Button with text content.",
		  demo_card(
			ui,
			xtk_button(
			  ui, "Standard button", (XtkButtonDesc) {.on_click = xtk_msg(MSG_CLICK), .disabled = m->button_disabled}
			),
			xtk_fmt(ui, "Clicked %d time%s", m->clicks, m->clicks == 1 ? "" : "s"),
			xtk_checkbox(
			  ui, "Disable button",
			  (XtkToggleDesc) {.checked = m->button_disabled, .on_change = xtk_msg(MSG_BUTTON_DISABLE)}
			)
		  )
		),
		example(
		  ui, "Built-in styles applied to Button.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				xtk_kids(
				  xtk_button(ui, "Accent style button", (XtkButtonDesc) {.style = FLUX_BUTTON_ACCENT}),
				  xtk_button(ui, "Standard style button", (XtkButtonDesc) {0}),
				  xtk_button(ui, "Subtle style button", (XtkButtonDesc) {.style = FLUX_BUTTON_SUBTLE}),
				  xtk_button(ui, "Text style button", (XtkButtonDesc) {.style = FLUX_BUTTON_TEXT}))
			  ))
		  )
		),
		example(
		  ui, "A Button with graphical content.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				xtk_kids(
				  xtk_button(ui, "Save", (XtkButtonDesc) {.icon = "Save"}),
				  xtk_button(ui, "Share", (XtkButtonDesc) {.icon = "Share"}),
				  xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Mail"}))
			  ))
		  )
		))
	);
}

XtkEl *page_split(XtkUi *ui, Model const *m) {
	static char const *const kActions [] = {"(nothing yet)", "Saved", "Saved as...", "Saved all"};
	int action = (m->split_action >= 0 && m->split_action <= 3) ? m->split_action : 0;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "SplitButton",
		  "The left zone fires the default action; the chevron opens a menu of alternatives."
		),
		example(
		  ui, "A SplitButton for saving with alternatives.",
		  demo_card(
			ui,
			xtk_split_button(
			  ui, "Save",
			  (XtkSplitDesc) {
				.icon     = "Save",
				.on_click = xtk_msg(MSG_SPLIT_PRIMARY),
				.items =
				  (XtkMenuItemDesc []) {
					{.label = "Save as...", .on_click = xtk_msg_i(MSG_SPLIT_MENU, 0)},
					{.label = "Save all", .on_click = xtk_msg_i(MSG_SPLIT_MENU, 1)},
				  },
				.item_count = 2}
			),
			xtk_fmt(ui, "Last action: %s", kActions [action]), NULL
		  )
		))
	);
}

XtkEl *page_checkbox(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "CheckBox", "CheckBox controls let the user select a combination of binary options."),
		example(
		  ui, "A 2-state CheckBox.",
		  demo_card(
			ui,
			xtk_checkbox(
			  ui, "Two-state CheckBox", (XtkToggleDesc) {.checked = m->check_a, .on_change = xtk_msg(MSG_CHECK_A)}
			),
			m->check_a ? "Checked" : "Unchecked", NULL
		  )
		),
		example(
		  ui, "CheckBoxes can be disabled.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(
			  xtk_checkbox(
				ui, "Enabled option", (XtkToggleDesc) {.checked = m->check_b, .on_change = xtk_msg(MSG_CHECK_B)}
			  ),
			  xtk_checkbox(ui, "Disabled option", (XtkToggleDesc) {.disabled = true}),
			  xtk_checkbox(ui, "Disabled and checked", (XtkToggleDesc) {.checked = true, .disabled = true}))
		  )
		))
	);
}

XtkEl *page_radio(XtkUi *ui, Model const *m) {
	static char const *const kOptions [] = {"Option 1", "Option 2", "Option 3"};
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "RadioButton", "RadioButtons present a set of mutually exclusive options — the model holds the index."
		),
		example(
		  ui, "A group of RadioButtons.",
		  demo_card(
			ui,
			xtk_column(
			  ui, (XtkStackDesc) {.gap = 8},
			  xtk_kids(
				xtk_radio(ui, kOptions [0], (XtkToggleDesc) {.checked = m->radio == 0, .on_change = xtk_msg(MSG_RADIO_0)}),
				xtk_radio(ui, kOptions [1], (XtkToggleDesc) {.checked = m->radio == 1, .on_change = xtk_msg(MSG_RADIO_1)}),
				xtk_radio(ui, kOptions [2], (XtkToggleDesc) {.checked = m->radio == 2, .on_change = xtk_msg(MSG_RADIO_2)}))
			),
			xtk_fmt(ui, "You selected %s", kOptions [m->radio]), NULL
		  )
		))
	);
}

XtkEl *page_combo(XtkUi *ui, Model const *m) {
	static char const *const kColors [] = {"Blue", "Green", "Red", "Yellow"};
	static char const *const kFonts []  = {
	   "Arial",          "Bahnschrift", "Cambria",  "Candara",  "Comic Sans MS", "Consolas",       "Constantia",
	   "Corbel",         "Courier New", "Ebrima",   "Gabriola", "Gadugi",        "Georgia",        "Impact",
	   "Lucida Console", "MS Gothic",   "MV Boli",  "Marlett",  "Segoe Print",   "Segoe Script",   "Segoe UI",
	   "SimSun",         "Sylfaen",     "Symbol",   "Tahoma",   "Times New Roman", "Trebuchet MS", "Verdana",
	};
	int const count      = ( int ) (sizeof(kColors) / sizeof(kColors [0]));
	int const font_count = ( int ) (sizeof(kFonts) / sizeof(kFonts [0]));
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ComboBox", "Use a ComboBox to let the user pick one item from a drop-down list."),
		example(
		  ui, "A ComboBox with items defined inline.",
		  demo_card(
			ui,
			xtk_combo_box(
			  ui, "Pick a color",
			  (XtkComboDesc) {.items = kColors, .count = count, .selected = m->combo_sel, .on_select = xtk_msg(MSG_COMBO)}
			),
			m->combo_sel >= 0 ? xtk_fmt(ui, "Selected: %s", kColors [m->combo_sel]) : "Nothing selected yet", NULL
		  )
		),
		example(
		  ui, "A ComboBox with a long scrolling list.",
		  demo_card(
			ui,
			xtk_combo_box(
			  ui, "Pick a font",
			  (XtkComboDesc) {
				.items = kFonts, .count = font_count, .selected = m->combo_font, .on_select = xtk_msg(MSG_COMBO_FONT)}
			),
			m->combo_font >= 0 ? xtk_fmt(ui, "Selected: %s", kFonts [m->combo_font]) : "Nothing selected yet", NULL
		  )
		))
	);
}

XtkEl *page_slider(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Slider", "Use a Slider when users should be able to set a value along a continuous or stepped range."
		),
		example(
		  ui, "A simple Slider.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(ui, (XtkSliderDesc) {.value = m->slider_basic, .on_change = xtk_msg(MSG_SLIDER_BASIC)}), 300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_basic), NULL
		  )
		),
		example(
		  ui, "A Slider with range and steps specified.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .min          = 500,
				  .max          = 1000,
				  .value        = m->slider_step,
				  .step         = 10,
				  .small_change = 10,
				  .large_change = 100,
				  .on_change    = xtk_msg(MSG_SLIDER_STEP)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_step), NULL
		  )
		),
		example(
		  ui, "A Slider with tick marks.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .value          = m->slider_tick,
				  .tick           = 20,
				  .tick_placement = FLUX_TICK_OUTSIDE,
				  .snap_to_ticks  = true,
				  .on_change      = xtk_msg(MSG_SLIDER_TICK)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_tick), NULL
		  )
		))
	);
}

XtkEl *page_toggle(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ToggleSwitch", "Use a ToggleSwitch for binary on/off settings that take effect immediately."),
		example(
		  ui, "A simple ToggleSwitch.",
		  demo_card(
			ui,
			xtk_row(
			  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			  xtk_kids(
				xtk_switch(ui, (XtkToggleDesc) {.checked = m->light_on, .on_change = xtk_msg(MSG_LIGHT)}),
				xtk_text(ui, m->light_on ? "On" : "Off", (XtkTextDesc) {0}))
			),
			m->light_on ? "The lights are on." : "The lights are off.", NULL
		  )
		))
	);
}
