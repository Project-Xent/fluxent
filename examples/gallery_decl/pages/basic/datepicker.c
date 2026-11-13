#include "gallery.h"

static char const *date_text(XtkUi *ui, int date) {
	if (date == 0) return "(no date)";
	return xtk_fmt(ui, "Picked: %04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

XtkEl *page_date_picker(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "DatePicker",
		  "A button that shows the date in the system locale's order. It opens a flyout of looping selectors, and "
		  "the date is only committed when the check button (or Enter) accepts it."
		),
		example(
		  ui, "A DatePicker with a header. Escape or a click outside dismisses it unchanged.",
		  demo_card(
			ui,
			xtk_date_picker(
			  ui,
			  (XtkDatePickerDesc) {.header = "Pick a date", .date = m->date_pick, .change = xtk_msg(MSG_DATE_PICKER)}
			),
			date_text(ui, m->date_pick), NULL
		  )
		),
		example(
		  ui, "With no date each column shows its name.",
		  demo_card(ui, xtk_date_picker(ui, (XtkDatePickerDesc) {.header = "Birthday"}), NULL, NULL)
		),
		example(
		  ui, "Years are limited to a range with MinYear and MaxYear.",
		  demo_card(
			ui,
			xtk_date_picker(
			  ui,
			  (XtkDatePickerDesc) {
				.header = "2024 to 2030", .date = xtk_date(2027, 6, 15), .minyear = 2024, .maxyear = 2030}
			),
			NULL, NULL
		  )
		),
		example(
		  ui, "A disabled DatePicker.",
		  demo_card(
			ui, xtk_date_picker(ui, (XtkDatePickerDesc) {.header = "Disabled", .date = m->date_pick, .disabled = true}),
			NULL, NULL
		  )
		)
	  )
	);
}
