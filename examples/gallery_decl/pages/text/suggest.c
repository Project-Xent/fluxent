#include "gallery.h"
#include <string.h>

static char const *const kCities [] = {
  "Amsterdam", "Athens", "Auckland", "Bangkok", "Barcelona", "Beijing", "Berlin", "Boston", "Brussels", "Budapest",
  "Cairo", "Chicago", "Dublin", "Helsinki", "Istanbul", "Jakarta", "Lisbon", "London", "Madrid", "Melbourne", "Mumbai",
  "Munich", "Nairobi", "Oslo", "Paris", "Prague", "Rome", "Seattle", "Seoul", "Shanghai", "Singapore", "Stockholm",
  "Sydney", "Tokyo", "Toronto", "Vienna", "Warsaw", "Zurich"};

#define CITY_COUNT ( int ) (sizeof(kCities) / sizeof(kCities [0]))

static char asb_lower(char c) {
	if (c >= 'A' && c <= 'Z') return ( char ) (c + 32);
	return c;
}

static bool asb_prefix_ci(char const *query, char const *city) {
	for (int k = 0; query [k]; k++)
		if (!city [k] || asb_lower(query [k]) != asb_lower(city [k])) return false;
	return true;
}

static int asb_filter(XtkUi *ui, char const *query, char const **out, int cap) {
	if (!query || !query [0]) return 0;
	int n = 0;
	for (int i = 0; i < CITY_COUNT && n < cap; i++) {
		if (!asb_prefix_ci(query, kCities [i])) continue;
		out [n++] = xtk_strdup(ui, kCities [i]);
	}
	return n;
}

XtkEl *page_autosuggest(XtkUi *ui, Model const *m) {
	char const *filtered [8];
	int         count  = asb_filter(ui, m->asb_text, filtered, 8);
	char const *status = m->asb_query ? xtk_fmt(ui, "Submitted: %s", m->asb_query) : "Type a city name.";

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "AutoSuggestBox",
		  "Typing posts the text; the model filters its data; the next frame's "
		  "suggestions open the list. Down/Up preview, Enter or the query button "
		  "submit."
		),
		example(
		  ui, "Prefix search over city names.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12},
			xtk_kids(
			  xtk_sized(
				xtk_suggest(
				  ui, "Search cities...",
				  (XtkAutoSuggestDesc) {
					.content     = m->asb_text,
					.suggestions = filtered,
					.nitem       = count,
					.query_icon  = "Find",
					.on_text     = xtk_msg(MSG_ASB_TEXT),
					.query       = xtk_msg(MSG_ASB_QUERY),
					.on_chosen   = xtk_msg(MSG_ASB_CHOSEN),
				  }
				),
				320.0f, NAN
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13})
			)
		  )
		)
	  )
	);
}
