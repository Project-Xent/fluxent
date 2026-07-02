/**
 * @file page_list.c
 * @brief ListView page: a virtualized 100,000-row list. Only the rows in
 * the viewport exist as nodes; scrolling recycles them.
 */
#include "gallery.h"

#define LIST_ROWS 100000

static char const *const kFirst [] = {"Ada",  "Grace", "Alan",   "Edsger", "Donald", "Barbara",
                                      "John", "Ken",   "Dennis", "Bjarne", "Anders", "Lin"};
static char const *const kLast []  = {"Lovelace", "Hopper",     "Turing",  "Dijkstra", "Knuth", "Liskov",
                                      "Backus",   "Thompson",   "Ritchie", "Stroustrup", "Hejlsberg", "Xu"};

static XtkEl *list_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	char const *last  = kLast [(index / 7) % (sizeof(kLast) / sizeof(kLast [0]))];
	return xtk_row(
	  ui, (XtkStackDesc) {.gap = 12},
	  (XtkEl *[]) {
		xtk_sized(xtk_text(ui, xtk_fmt(ui, "%d", index), (XtkTextDesc) {.size = 12}), 64, NAN),
		xtk_text(ui, xtk_fmt(ui, "%s %s", first, last), (XtkTextDesc) {0}), XTK_END}
	);
}

XtkEl *page_listview(XtkUi *ui, Model const *m) {
	char const *status = m->list_sel >= 0 ? xtk_fmt(ui, "Selected row: %d", m->list_sel) : "Nothing selected yet.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(
		  ui, "ListView",
		  "A virtualized list: 100,000 rows, but only the viewport's worth of controls exist. "
		  "Scrolling recycles rows in place."
		),
		example(
		  ui, "Click a row to select it.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			(XtkEl *[]) {
			  xtk_sized(
				xtk_list_view(
				  ui, (XtkListDesc) {
				        .count       = LIST_ROWS,
				        .item_height = 40.0f,
				        .item        = list_row,
				        .selected    = m->list_sel,
				        .on_select   = xtk_msg(MSG_LIST_SELECT),
				      }
				),
				NAN, 420.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13}), XTK_END}
		  )
		),
		XTK_END}
	);
}
