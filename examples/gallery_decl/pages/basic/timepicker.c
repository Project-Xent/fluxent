#include "gallery.h"

static char const *time_text(XtkUi *ui, int minutes) {
	return xtk_fmt(ui, "Picked: %02d:%02d", minutes / 60, minutes % 60);
}

XtkEl *page_time_picker(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "TimePicker",
		  "Like the DatePicker, a button over a flyout of looping selectors. The clock follows the system unless "
		  "the control says 12 or 24 hours, and the minutes step by the minute increment."
		),
		example(
		  ui, "A 12-hour TimePicker.",
		  demo_card(
			ui,
			xtk_time_picker(
			  ui,
			  (XtkTimePickerDesc) {
				.header   = "Alarm",
				.has_time = true,
				.minutes  = m->time_pick,
				.clock    = XTK_CLOCK_12H,
				.change   = xtk_msg(MSG_TIME_PICKER)}
			),
			time_text(ui, m->time_pick), NULL
		  )
		),
		example(
		  ui, "A 24-hour clock with a 15 minute increment.",
		  demo_card(
			ui,
			xtk_time_picker(
			  ui,
			  (XtkTimePickerDesc) {
				.header   = "Departure",
				.has_time = true,
				.minutes  = m->time_pick_24,
				.clock    = XTK_CLOCK_24H,
				.step     = 15,
				.change   = xtk_msg(MSG_TIME_PICKER_24)}
			),
			time_text(ui, m->time_pick_24), NULL
		  )
		),
		example(
		  ui, "With no time each column shows its name.",
		  demo_card(ui, xtk_time_picker(ui, (XtkTimePickerDesc) {.header = "Reminder"}), NULL, NULL)
		),
		example(
		  ui, "A disabled TimePicker.",
		  demo_card(
			ui,
			xtk_time_picker(
			  ui, (XtkTimePickerDesc) {.header = "Disabled", .has_time = true, .minutes = 8 * 60, .disabled = true}
			),
			NULL, NULL
		  )
		)
	  )
	);
}
