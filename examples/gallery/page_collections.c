/**
 * @file page_collections.c
 * @brief GridView / ListBox / FlipView+PipsPager / AutoSuggestBox pages.
 */
#include "gallery.h"

#include <string.h>

/* ---------------------------------------------------------------- GridView */

static XtkEl *grid_cell(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_column(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER, .justify = XENT_FLEX_JUSTIFY_CENTER},
	  xtk_kids(
		xtk_text(ui, xtk_fmt(ui, "%d", index), (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, xtk_fmt(ui, "Item %d", index), (XtkTextDesc) {.size = 11}))
	);
}

XtkEl *page_gridview(XtkUi *ui, Model const *m) {
	char const *status = m->grid_sel >= 0 ? xtk_fmt(ui, "Selected cell: %d", m->grid_sel) : "Nothing selected yet.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "GridView",
		  "The same virtualized items host wrapping cells across auto-fit columns. "
		  "Arrow keys navigate in 2D; the selection ring is the WinUI accent border."
		),
		example(
		  ui, "10,000 uniform cells; columns follow the width.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_grid_view(
				  ui, (XtkListDesc) {
				        .count       = 10000,
				        .item_height = 104.0f,
				        .item_width  = 152.0f,
				        .item        = grid_cell,
				        .selected    = m->grid_sel,
				        .on_select   = xtk_msg(MSG_GRID_SELECT),
				      }
				),
				NAN, 420.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13}))
		  )
		))
	);
}

/* ---------------------------------------------------------------- ListBox */

static char const *const kFonts [] = {"Arial",  "Calibri",  "Cambria", "Consolas", "Courier New",
                                      "Georgia", "Segoe UI", "Tahoma",  "Times New Roman", "Verdana"};
#define FONT_COUNT ( int ) (sizeof(kFonts) / sizeof(kFonts [0]))

static XtkEl *listbox_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, kFonts [index], (XtkTextDesc) {0});
}

XtkEl *page_listbox(XtkUi *ui, Model const *m) {
	char const *status
	  = m->listbox_sel >= 0 ? xtk_fmt(ui, "Font: %s", kFonts [m->listbox_sel]) : "Pick a font.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ListBox", "The bordered list surface with accent-filled selection."),
		example(
		  ui, "Selection fills the row with the accent at WinUI's 0.6/0.8/0.9 opacities.",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 16},
			xtk_kids(
			  xtk_sized(
				xtk_list_box(
				  ui, (XtkListDesc) {
				        .count       = FONT_COUNT,
				        .item_height = 40.0f,
				        .item        = listbox_row,
				        .selected    = m->listbox_sel,
				        .on_select   = xtk_msg(MSG_LISTBOX_SELECT),
				      }
				),
				260.0f, 320.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13}))
		  )
		))
	);
}

/* --------------------------------------------------- FlipView + PipsPager */

static XtkEl *flip_page_el(XtkUi *ui, int i) {
	static char const *const kTitles [] = {"Sunrise", "Forest", "Ocean", "Dunes", "Aurora"};
	return xtk_card(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER, .justify = XENT_FLEX_JUSTIFY_CENTER},
	  xtk_kids(
		xtk_text(ui, kTitles [i], (XtkTextDesc) {.size = 32, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, xtk_fmt(ui, "Page %d of 5", i + 1), (XtkTextDesc) {.size = 13}))
	);
}

XtkEl *page_flipview(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "FlipView",
		  "One page at a time: hover for the nav arrows, use the wheel or arrow keys, "
		  "or drive it from the PipsPager below — both post the same message."
		),
		example(
		  ui, "FlipView + PipsPager sharing one selected index.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			xtk_kids(
			  xtk_sized(
				xtk_flip_view(
				  ui, (XtkFlipDesc) {.selected = m->flip_page, .on_select = xtk_msg(MSG_FLIP_SELECT)},
				  xtk_kids(
					flip_page_el(ui, 0), flip_page_el(ui, 1), flip_page_el(ui, 2), flip_page_el(ui, 3),
					flip_page_el(ui, 4))
				),
				560.0f, 300.0f
			  ),
			  xtk_pips_pager(
				ui, (XtkPipsDesc) {
				      .count     = 5,
				      .selected  = m->flip_page,
				      .nav       = XTK_PIPS_NAV_ON_HOVER,
				      .on_select = xtk_msg(MSG_FLIP_SELECT),
				    }
			  ))
		  )
		))
	);
}

/* ---------------------------------------------------------- AutoSuggestBox */

static char const *const kCities []
  = {"Amsterdam", "Athens",   "Auckland", "Bangkok",  "Barcelona", "Beijing",  "Berlin",   "Boston",
     "Brussels",  "Budapest", "Cairo",    "Chicago",  "Dublin",    "Helsinki", "Istanbul", "Jakarta",
     "Lisbon",    "London",   "Madrid",   "Melbourne", "Mumbai",   "Munich",   "Nairobi",  "Oslo",
     "Paris",     "Prague",   "Rome",     "Seattle",  "Seoul",     "Shanghai", "Singapore", "Stockholm",
     "Sydney",    "Tokyo",    "Toronto",  "Vienna",   "Warsaw",    "Zurich"};
#define CITY_COUNT ( int ) (sizeof(kCities) / sizeof(kCities [0]))

/* Case-insensitive prefix filter into the frame arena. */
static int asb_filter(XtkUi *ui, char const *query, char const **out, int cap) {
	if (!query || !query [0]) return 0;
	int n = 0;
	for (int i = 0; i < CITY_COUNT && n < cap; i++) {
		bool match = true;
		for (int k = 0; query [k] && match; k++) {
			char a = query [k], b = kCities [i][k];
			if (a >= 'A' && a <= 'Z') a += 32;
			if (b >= 'A' && b <= 'Z') b += 32;
			if (!kCities [i][k] || a != b) match = false;
		}
		if (match) out [n++] = xtk_strdup(ui, kCities [i]);
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
		  "suggestions open the list. Down/Up preview, Enter or the query button submit."
		),
		example(
		  ui, "Prefix search over city names.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12},
			xtk_kids(
			  xtk_sized(
				xtk_auto_suggest(
				  ui, "Search cities...",
				  (XtkAutoSuggestDesc) {
				    .suggestions = filtered,
				    .count       = count,
				    .query_icon  = "Find",
				    .on_text     = xtk_msg(MSG_ASB_TEXT),
				    .on_query    = xtk_msg(MSG_ASB_QUERY),
				    .on_chosen   = xtk_msg(MSG_ASB_CHOSEN),
				  }
				),
				320.0f, NAN
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13}))
		  )
		))
	);
}