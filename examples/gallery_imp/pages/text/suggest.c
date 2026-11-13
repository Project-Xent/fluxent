#include "app.h"

#include <ctype.h>

static char const *const kCities [] = {
  "Amsterdam", "Athens", "Auckland", "Bangkok", "Barcelona", "Beijing", "Berlin", "Boston", "Brussels", "Budapest",
  "Cairo", "Chicago", "Dublin", "Helsinki", "Istanbul", "Jakarta", "Lisbon", "London", "Madrid", "Melbourne", "Mumbai",
  "Munich", "Nairobi", "Oslo", "Paris", "Prague", "Rome", "Seattle", "Seoul", "Shanghai", "Singapore", "Stockholm",
  "Sydney", "Tokyo", "Toronto", "Vienna", "Warsaw", "Zurich"};

#define CITY_COUNT ( int ) (sizeof(kCities) / sizeof(kCities [0]))

static bool prefix_ci(char const *query, char const *city) {
	for (int k = 0; query [k]; k++)
		if (!city [k] || tolower(( unsigned char ) query [k]) != tolower(( unsigned char ) city [k])) return false;
	return true;
}

static int filter(char const *query, char const **out) {
	int n = 0;
	for (int i = 0; query && query [0] && i < CITY_COUNT && n < 8; i++)
		if (prefix_ci(query, kCities [i])) out [n++] = kCities [i];
	return n;
}

static char const *query_text(GalleryApp *g) {
	return g->model.asb_query ? gallery_fmt(g, "Submitted: %s", g->model.asb_query) : "Type a city name.";
}

static void suggest_sync(GalleryApp *g) {
	char const *found [8];
	xtk_ctrl_setitems(&g->scene, g->live_control [MSG_ASB_TEXT], found, filter(g->model.asb_text, found));
	gallery_settext(g, g->live_output [MSG_ASB_QUERY], query_text(g));
}

bool page_autosuggest_build(PageBuild *b) {
	page_sync(b, suggest_sync);
	char const *text   = b->app->model.asb_text;
	XtkNodeRef  parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "AutoSuggestBox",
	  "Typing posts the text; the model filters its data; the next frame's suggestions open the list. Down/Up preview, "
	  "Enter or the query button submit."
	);
	XtkNodeRef col = xtk_ref_local(
	  ui_column(&b->txn, page_example(b, parent, "Prefix search over city names."), 12.0f, XENT_FLEX_ALIGN_START)
	);
	XtkLocalId box = xtk_suggest_create(
	  &b->txn, col,
	  &(XtkSuggestProps) {
		.set         = XTK_SUGGEST_W_MASK
	                 | XTK_SUGGEST_PLACEHOLDER_MASK
	                 | XTK_SUGGEST_ICON_MASK
	                 | (text ? XTK_SUGGEST_TEXT_MASK : 0u),
		.w           = 320.0f,
		.placeholder = "Search cities...",
		.icon        = "Find",
		.text        = text,
	  }
	);
	char const *found [8];
	XtkNodeRef  ref = xtk_ref_local(box);
	xtk_txn_setitems(&b->txn, ref, found, filter(text, found));
	xtk_suggest_onchange(&b->txn, ref, page_post, page_bind(b, MSG_ASB_TEXT));
	xtk_suggest_onquery(&b->txn, ref, page_post, page_bind(b, MSG_ASB_QUERY));
	xtk_suggest_onselect(&b->txn, ref, page_post, page_bind(b, MSG_ASB_CHOSEN));
	page_keep(b, MSG_ASB_TEXT, box);
	page_keepout(b, MSG_ASB_QUERY, page_note(b, col, query_text(b->app)));
	return true;
}
