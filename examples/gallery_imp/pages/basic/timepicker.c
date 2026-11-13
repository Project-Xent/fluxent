#include "app.h"

static char const *time_text(GalleryApp *g, int minutes) {
	return gallery_fmt(g, "Picked: %02d:%02d", minutes / 60, minutes % 60);
}

static void time_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_TIME_PICKER], time_text(g, g->model.time_pick));
	gallery_settext(g, g->live_output [MSG_TIME_PICKER_24], time_text(g, g->model.time_pick_24));
}

static XtkLocalId time_picker(PageBuild *b, XtkNodeRef parent, XtkTimePickerProps props) {
	props.set |= XTK_TIMEPICKER_HEADER_MASK | XTK_TIMEPICKER_TIME_MASK | XTK_TIMEPICKER_CLOCK_MASK;
	if (props.step) props.set |= XTK_TIMEPICKER_STEP_MASK;
	return xtk_timepicker_create(&b->txn, parent, &props);
}

static void time_example(PageBuild *b, XtkNodeRef parent, char const *caption, XtkTimePickerProps props, int tag) {
	PageDemo   demo = page_demo(b, page_example(b, parent, caption));
	XtkLocalId pick = time_picker(b, demo.items, props);
	xtk_timepicker_onvalue(&b->txn, xtk_ref_local(pick), page_post, page_bind(b, tag));
	page_keepout(b, tag, page_output(b, demo.row, time_text(b->app, props.time)));
}

bool page_time_picker_build(PageBuild *b) {
	page_sync(b, time_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "TimePicker",
	  "Like the DatePicker, a button over a flyout of looping selectors. The clock follows the system unless the "
	  "control says 12 or 24 hours, and the minutes step by the minute increment."
	);
	time_example(
	  b, parent, "A 12-hour TimePicker.",
	  (XtkTimePickerProps) {.header = "Alarm", .time = m->time_pick, .clock = XTK_CLOCK_12H}, MSG_TIME_PICKER
	);
	time_example(
	  b, parent, "A 24-hour clock with a 15 minute increment.",
	  (XtkTimePickerProps) {.header = "Departure", .time = m->time_pick_24, .clock = XTK_CLOCK_24H, .step = 15},
	  MSG_TIME_PICKER_24
	);
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "With no time each column shows its name."));
		time_picker(b, demo.items, (XtkTimePickerProps) {.header = "Reminder", .time = -1});
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "A disabled TimePicker."));
		ui_enable(
		  &b->txn, time_picker(b, demo.items, (XtkTimePickerProps) {.header = "Disabled", .time = 8 * 60}), false
		);
	}
	return true;
}
