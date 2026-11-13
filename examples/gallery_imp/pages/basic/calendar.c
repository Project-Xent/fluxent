#include "app.h"

static char const *date_text(GalleryApp *g, int date) {
	if (date == 0) return "Nothing selected";
	return gallery_fmt(g, "Selected: %04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

static char const *multi_text(GalleryApp *g) {
	return gallery_fmt(g, "%d date%s selected", g->model.cal_multi_count, g->model.cal_multi_count == 1 ? "" : "s");
}

static void calendar_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_CALENDAR], date_text(g, g->model.cal_date));
	gallery_settext(g, g->live_output [MSG_CALENDAR_MULTI], multi_text(g));
}

static XtkLocalId calendar(PageBuild *b, XtkNodeRef parent, XtkCalendarProps props) {
	props.set |= XTK_CALENDAR_DATE_MASK | XTK_CALENDAR_MODE_MASK;
	if (props.mindate) props.set |= XTK_CALENDAR_MINDATE_MASK | XTK_CALENDAR_MAXDATE_MASK;
	return xtk_calendar_create(&b->txn, parent, &props);
}

static void calendar_example(PageBuild *b, XtkNodeRef parent, char const *caption, XtkCalendarProps props, int tag) {
	PageDemo   demo = page_demo(b, page_example(b, parent, caption));
	XtkLocalId cal  = calendar(b, demo.items, props);
	xtk_calendar_ondate(&b->txn, xtk_ref_local(cal), page_post, page_bind(b, tag));
	char const *out = tag == MSG_CALENDAR ? date_text(b->app, props.date) : multi_text(b->app);
	page_keepout(b, tag, page_output(b, demo.row, out));
}

bool page_calendar_build(PageBuild *b) {
	page_sync(b, calendar_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "CalendarView",
	  "A month grid that scrolls through MinDate..MaxDate. The header button zooms out to the year and decade views, "
	  "the arrows page by month, year or decade, and Ctrl+Up / Ctrl+Down switch views from the keyboard."
	);
	calendar_example(
	  b, parent, "A CalendarView with single selection (the default).",
	  (XtkCalendarProps) {.date = b->app->model.cal_date, .mode = XTK_CALENDAR_SINGLE}, MSG_CALENDAR
	);
	calendar_example(
	  b, parent, "SelectionMode Multiple: each click toggles a date.",
	  (XtkCalendarProps) {.mode = XTK_CALENDAR_MULTIPLE}, MSG_CALENDAR_MULTI
	);
	{
		PageDemo demo = page_demo(
		  b, page_example(b, parent, "MinDate and MaxDate limit the range; the arrows disable at the ends.")
		);
		calendar(b, demo.items, (XtkCalendarProps) {.date = 20260415, .mindate = 20260310, .maxdate = 20260720});
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "A disabled CalendarView."));
		ui_enable(&b->txn, calendar(b, demo.items, (XtkCalendarProps) {0}), false);
	}
	return true;
}
