#include "app.h"
#include "host/flux_prop_apply.h"
#include <math.h>
#include <stdio.h>
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

static int asb_filter(char const *query, char const **out, int cap) {
	if (!query || !query [0]) return 0;
	int n = 0;
	for (int i = 0; i < CITY_COUNT && n < cap; i++) {
		if (!asb_prefix_ci(query, kCities [i])) continue;
		out [n++] = kCities [i];
	}
	return n;
}

static void on_asb_text(void *ctx, char const *text) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .ptr = ( void * ) text});
}

static void on_asb_query(void *ctx, char const *text) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .ptr = ( void * ) text});
}

static void on_asb_chosen(void *ctx, int idx) {
	GalleryBind *b = ctx;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

void page_autosuggest(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_autosuggest_sync);
	Model const *m = &g->model;
	char const  *filtered [8];
	int          count  = asb_filter(m->asb_text, filtered, 8);
	char const  *status = m->asb_query ? gallery_fmt(g, "Submitted: %s", m->asb_query) : "Type a city name.";
	g_page_header(
	  g, parent, "AutoSuggestBox",
	  "Typing posts the text; the model filters its data; the next frame's "
	  "suggestions open the list. Down/Up preview, Enter or the query button submit."
	);
	{
		XentNodeId   ex          = g_example(g, parent, "Prefix search over city names.");
		XentNodeId   col         = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		GalleryBind *text_bind   = gallery_bind(g, MSG_ASB_TEXT);
		GalleryBind *query_bind  = gallery_bind(g, MSG_ASB_QUERY);
		GalleryBind *chosen_bind = gallery_bind(g, MSG_ASB_CHOSEN);
		XentNodeId   asb         = flux_suggest_init(&(FluxAutoSuggestDesc) {
		  .ctx         = gallery_ctx(g),
		  .store       = gallery_store(g),
		  .parent      = col,
		  .app         = g->flux,
		  .window      = flux_app_window(g->flux),
		  .text        = flux_app_text(g->flux),
		  .theme       = flux_app_theme(g->flux),
		  .placeholder = "Search cities...",
		  .query_icon  = "Find",
		  .on_text     = on_asb_text,
		  .on_query    = on_asb_query,
		  .on_chosen   = on_asb_chosen,
		  .text_ctx    = text_bind,
		  .query_ctx   = query_bind,
		  .chosen_ctx  = chosen_bind,
		});
		xent_setw(gallery_ctx(g), asb, 320.0f);
		flux_suggest_set_suggestions(gallery_store(g), asb, filtered, count);
		g->live_control [MSG_ASB_TEXT] = asb;
		g->live_output [MSG_ASB_QUERY] = g_text(g, col, status, 13.0f, FLUX_FONT_REGULAR);
	}
}

void page_autosuggest_sync(GalleryApp *g) {
	char const *filtered [8];
	int         count = asb_filter(g->model.asb_text, filtered, 8);
	flux_suggest_set_suggestions(gallery_store(g), g->live_control [MSG_ASB_TEXT], filtered, count);

	char const *status = g->model.asb_query ? gallery_fmt(g, "Submitted: %s", g->model.asb_query) : "Type a city name.";
	xent_settext(gallery_ctx(g), g->live_output [MSG_ASB_QUERY], status);
}
