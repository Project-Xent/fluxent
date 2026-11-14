#include "gallery.h"

static char const *date_text(XtkUi *ui, int date) {
	if (date == 0) return "Nothing selected";
	return xtk_fmt(ui, "Selected: %04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

XtkEl *page_calendar(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "CalendarView",
		  "A month grid that scrolls through MinDate..MaxDate. The header button zooms out to the year and decade "
		  "views, the arrows page by month, year or decade, and Ctrl+Up / Ctrl+Down switch views from the keyboard."
		),
		example(
		  ui, "A CalendarView with single selection (the default).",
		  demo_card(
			ui,
			xtk_calendar_view(
			  ui, (XtkCalendarDesc) {.date = m->cal_date, .mode = XTK_CALENDAR_SINGLE, .select = xtk_msg(MSG_CALENDAR)}
			),
			date_text(ui, m->cal_date), NULL
		  )
		),
		example(
		  ui, "SelectionMode Multiple: each click toggles a date.",
		  demo_card(
			ui,
			xtk_calendar_view(
			  ui, (XtkCalendarDesc) {.mode = XTK_CALENDAR_MULTIPLE, .select = xtk_msg(MSG_CALENDAR_MULTI)}
			),
			xtk_fmt(ui, "%d date%s selected", m->cal_multi_count, m->cal_multi_count == 1 ? "" : "s"), NULL
		  )
		),
		example(
		  ui, "MinDate and MaxDate limit the range; the arrows disable at the ends.",
		  demo_card(
			ui,
			xtk_calendar_view(
			  ui,
			  (XtkCalendarDesc) {
				.date = xtk_date(2026, 4, 15), .mindate = xtk_date(2026, 3, 10), .maxdate = xtk_date(2026, 7, 20)}
			),
			NULL, NULL
		  )
		),
		example(
		  ui, "A disabled CalendarView.",
		  demo_card(ui, xtk_calendar_view(ui, (XtkCalendarDesc) {.disabled = true}), NULL, NULL)
		)
	  )
	);
}
