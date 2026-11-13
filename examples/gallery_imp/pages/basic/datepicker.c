#include "app.h"

static char const *date_text(GalleryApp *g, int date) {
	if (date == 0) return "(no date)";
	return gallery_fmt(g, "Picked: %04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

static void date_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_DATE_PICKER], date_text(g, g->model.date_pick));
}

static XtkLocalId date_picker(PageBuild *b, XtkNodeRef parent, XtkDatePickerProps props) {
	props.set |= XTK_DATEPICKER_HEADER_MASK | XTK_DATEPICKER_DATE_MASK;
	if (props.minyear) props.set |= XTK_DATEPICKER_MINYEAR_MASK | XTK_DATEPICKER_MAXYEAR_MASK;
	return xtk_datepicker_create(&b->txn, parent, &props);
}

bool page_date_picker_build(PageBuild *b) {
	page_sync(b, date_sync);
	int        date   = b->app->model.date_pick;
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "DatePicker",
	  "A button that shows the date in the system locale's order. It opens a flyout of looping selectors, and the "
	  "date is only committed when the check button (or Enter) accepts it."
	);
	{
		PageDemo demo = page_demo(
		  b, page_example(b, parent, "A DatePicker with a header. Escape or a click outside dismisses it unchanged.")
		);
		XtkLocalId pick = date_picker(b, demo.items, (XtkDatePickerProps) {.header = "Pick a date", .date = date});
		xtk_datepicker_onvalue(&b->txn, xtk_ref_local(pick), page_post, page_bind(b, MSG_DATE_PICKER));
		page_keepout(b, MSG_DATE_PICKER, page_output(b, demo.row, date_text(b->app, date)));
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "With no date each column shows its name."));
		date_picker(b, demo.items, (XtkDatePickerProps) {.header = "Birthday"});
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "Years are limited to a range with MinYear and MaxYear."));
		date_picker(
		  b, demo.items,
		  (XtkDatePickerProps) {.header = "2024 to 2030", .date = 20270615, .minyear = 2024, .maxyear = 2030}
		);
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "A disabled DatePicker."));
		ui_enable(
		  &b->txn, date_picker(b, demo.items, (XtkDatePickerProps) {.header = "Disabled", .date = date}), false
		);
	}
	return true;
}
