#include "gallery.h"

FluxEl *page_button(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "Button",
		  "The Button control provides a Click event to respond to user input. It posts an FxMsg; nothing calls " "back" " int" "o " "your" " cod" "e."
		),
		example(
		  ui, "A simple Button with text content.",
		  demo_card(
			ui,
			fx_button(
			  ui, "Standard FX button", (FxButtonDesc) {.on_click = fx_msg(MSG_CLICK), .disabled = m->button_disabled}
			),
			fx_fmt(ui, "Clicked %d time%s", m->clicks, m->clicks == 1 ? "" : "s"),
			fx_checkbox(
			  ui, "Disable button",
			  (FxToggleDesc) {.checked = m->button_disabled, .on_change = fx_msg(MSG_BUTTON_DISABLE)}
			)
		  )
		),
		example(
		  ui, "Built-in styles applied to Button.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_row(
				ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				(FluxEl *[]) {
				  fx_button(ui, "Accent style button", (FxButtonDesc) {.style = FLUX_BUTTON_ACCENT}),
				  fx_button(ui, "Standard style button", (FxButtonDesc) {0}),
				  fx_button(ui, "Subtle style button", (FxButtonDesc) {.style = FLUX_BUTTON_SUBTLE}),
				  fx_button(ui, "Text style button", (FxButtonDesc) {.style = FLUX_BUTTON_TEXT}), FX_END}
			  ),
			  FX_END}
		  )
		),
		example(
		  ui, "A Button with graphical content.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_row(
				ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
				(FluxEl *[]) {
				  fx_button(ui, "Save", (FxButtonDesc) {.icon = "Save"}),
				  fx_button(ui, "Share", (FxButtonDesc) {.icon = "Share"}),
				  fx_button(ui, NULL, (FxButtonDesc) {.icon = "Mail"}), FX_END}
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}

FluxEl *page_split(FxUi *ui, Model const *m) {
	static char const *const kActions [] = {"(nothing yet)", "Saved", "Saved as...", "Saved all"};
	int action = (m->split_action >= 0 && m->split_action <= 3) ? m->split_action : 0;
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "SplitButton",
		  "The left zone fires the default action; the chevron opens a menu of alternatives."
		),
		example(
		  ui, "A SplitButton for saving with alternatives.",
		  demo_card(
			ui,
			fx_split_button(
			  ui, "Save",
			  (FxSplitDesc) {
				.icon     = "Save",
				.on_click = fx_msg(MSG_SPLIT_PRIMARY),
				.items =
				  (FxMenuItemDesc []) {
					{.label = "Save as...", .on_click = fx_msg_i(MSG_SPLIT_MENU, 0)},
					{.label = "Save all", .on_click = fx_msg_i(MSG_SPLIT_MENU, 1)},
				  },
				.item_count = 2}
			),
			fx_fmt(ui, "Last action: %s", kActions [action]), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_checkbox(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "CheckBox", "CheckBox controls let the user select a combination of binary options."),
		example(
		  ui, "A 2-state CheckBox.",
		  demo_card(
			ui,
			fx_checkbox(
			  ui, "Two-state CheckBox", (FxToggleDesc) {.checked = m->check_a, .on_change = fx_msg(MSG_CHECK_A)}
			),
			m->check_a ? "Checked" : "Unchecked", NULL
		  )
		),
		example(
		  ui, "CheckBoxes can be disabled.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_checkbox(
				ui, "Enabled option", (FxToggleDesc) {.checked = m->check_b, .on_change = fx_msg(MSG_CHECK_B)}
			  ),
			  fx_checkbox(ui, "Disabled option", (FxToggleDesc) {.disabled = true}),
			  fx_checkbox(ui, "Disabled and checked", (FxToggleDesc) {.checked = true, .disabled = true}), FX_END}
		  )
		),
		FX_END}
	);
}

FluxEl *page_radio(FxUi *ui, Model const *m) {
	static char const *const kOptions [] = {"Option 1", "Option 2", "Option 3"};
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "RadioButton", "RadioButtons present a set of mutually exclusive options — the model holds the index."
		),
		example(
		  ui, "A group of RadioButtons.",
		  demo_card(
			ui,
			fx_column(
			  ui, (FxStackDesc) {.gap = 8},
			  (FluxEl *[]) {
				fx_radio(ui, kOptions [0], (FxToggleDesc) {.checked = m->radio == 0, .on_change = fx_msg(MSG_RADIO_0)}),
				fx_radio(ui, kOptions [1], (FxToggleDesc) {.checked = m->radio == 1, .on_change = fx_msg(MSG_RADIO_1)}),
				fx_radio(ui, kOptions [2], (FxToggleDesc) {.checked = m->radio == 2, .on_change = fx_msg(MSG_RADIO_2)}),
				FX_END}
			),
			fx_fmt(ui, "You selected %s", kOptions [m->radio]), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_combo(FxUi *ui, Model const *m) {
	static char const *const kColors [] = {"Blue", "Green", "Red", "Yellow"};
	static char const *const kFonts []  = {
	   "Arial",          "Bahnschrift", "Cambria",  "Candara",  "Comic Sans MS", "Consolas",       "Constantia",
	   "Corbel",         "Courier New", "Ebrima",   "Gabriola", "Gadugi",        "Georgia",        "Impact",
	   "Lucida Console", "MS Gothic",   "MV Boli",  "Marlett",  "Segoe Print",   "Segoe Script",   "Segoe UI",
	   "SimSun",         "Sylfaen",     "Symbol",   "Tahoma",   "Times New Roman", "Trebuchet MS", "Verdana",
	};
	int const count      = ( int ) (sizeof(kColors) / sizeof(kColors [0]));
	int const font_count = ( int ) (sizeof(kFonts) / sizeof(kFonts [0]));
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "ComboBox", "Use a ComboBox to let the user pick one item from a drop-down list."),
		example(
		  ui, "A ComboBox with items defined inline.",
		  demo_card(
			ui,
			fx_combo_box(
			  ui, "Pick a color",
			  (FxComboDesc) {.items = kColors, .count = count, .selected = m->combo_sel, .on_select = fx_msg(MSG_COMBO)}
			),
			m->combo_sel >= 0 ? fx_fmt(ui, "Selected: %s", kColors [m->combo_sel]) : "Nothing selected yet", NULL
		  )
		),
		example(
		  ui, "A ComboBox with a long scrolling list.",
		  demo_card(
			ui,
			fx_combo_box(
			  ui, "Pick a font",
			  (FxComboDesc) {
				.items = kFonts, .count = font_count, .selected = m->combo_font, .on_select = fx_msg(MSG_COMBO_FONT)}
			),
			m->combo_font >= 0 ? fx_fmt(ui, "Selected: %s", kFonts [m->combo_font]) : "Nothing selected yet", NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_slider(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(
		  ui, "Slider", "Use a Slider when users should be able to set a value along a continuous or stepped range."
		),
		example(
		  ui, "A simple Slider.",
		  demo_card(
			ui,
			fx_sized(
			  fx_slider(ui, (FxSliderDesc) {.value = m->slider_basic, .on_change = fx_msg(MSG_SLIDER_BASIC)}), 300, NAN
			),
			fx_fmt(ui, "%.0f", m->slider_basic), NULL
		  )
		),
		example(
		  ui, "A Slider with range and steps specified.",
		  demo_card(
			ui,
			fx_sized(
			  fx_slider(
				ui,
				(FxSliderDesc) {
				  .min          = 500,
				  .max          = 1000,
				  .value        = m->slider_step,
				  .step         = 10,
				  .small_change = 10,
				  .large_change = 100,
				  .on_change    = fx_msg(MSG_SLIDER_STEP)}
			  ),
			  300, NAN
			),
			fx_fmt(ui, "%.0f", m->slider_step), NULL
		  )
		),
		example(
		  ui, "A Slider with tick marks.",
		  demo_card(
			ui,
			fx_sized(
			  fx_slider(
				ui,
				(FxSliderDesc) {
				  .value          = m->slider_tick,
				  .tick           = 20,
				  .tick_placement = FLUX_TICK_OUTSIDE,
				  .snap_to_ticks  = true,
				  .on_change      = fx_msg(MSG_SLIDER_TICK)}
			  ),
			  300, NAN
			),
			fx_fmt(ui, "%.0f", m->slider_tick), NULL
		  )
		),
		FX_END}
	);
}

FluxEl *page_toggle(FxUi *ui, Model const *m) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "ToggleSwitch", "Use a ToggleSwitch for binary on/off settings that take effect immediately."),
		example(
		  ui, "A simple ToggleSwitch.",
		  demo_card(
			ui,
			fx_row(
			  ui, (FxStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			  (FluxEl *[]) {
				fx_switch(ui, (FxToggleDesc) {.checked = m->light_on, .on_change = fx_msg(MSG_LIGHT)}),
				fx_text(ui, m->light_on ? "On" : "Off", (FxTextDesc) {0}), FX_END}
			),
			m->light_on ? "The lights are on." : "The lights are off.", NULL
		  )
		),
		FX_END}
	);
}
