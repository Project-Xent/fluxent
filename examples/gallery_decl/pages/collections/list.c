#include "gallery.h"

#include <stdio.h>

#define LIST_ROWS 100000

static char const *const kFirst []
  = {"Ada", "Grace", "Alan", "Edsger", "Donald", "Barbara", "John", "Ken", "Dennis", "Bjarne", "Anders", "Lin"};

static char const *const kLast [] = {
  "Lovelace", "Hopper", "Turing", "Dijkstra", "Knuth", "Liskov", "Backus", "Thompson", "Ritchie", "Stroustrup",
  "Hejlsberg", "Xu"};

static XtkEl *list_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	char const *last  = kLast [(index / 7) % (sizeof(kLast) / sizeof(kLast [0]))];
	return xtk_row(
	  ui, (XtkStackDesc) {.gap = 12},
	  xtk_kids(
		xtk_sized(xtk_text(ui, xtk_fmt(ui, "%d", index), (XtkTextDesc) {.size = 12}), 64, NAN),
		xtk_text(ui, xtk_fmt(ui, "%s %s", first, last), (XtkTextDesc) {0})
	  )
	);
}

static char const *const kWords [] = {
  "layout", "measures", "every", "realized", "row", "while", "the", "rest", "of", "the", "list", "uses", "an",
  "average", "so", "scrolling", "stays", "smooth", "even", "when", "heights", "vary"};

static char const *note_text(XtkUi *ui, int index) {
	char buf [512];
	int  words = 3 + (index * 7) % 41, len = 0;
	for (int i = 0; i < words && len < ( int ) sizeof(buf) - 16; i++)
		len += snprintf(buf + len, sizeof(buf) - ( size_t ) len, i ? " %s" : "%s", kWords [(index + i * 5) % 22]);
	return xtk_fmt(ui, "%s.", buf);
}

static XtkEl *note_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 2, .padding = {0, 8, 0, 8},
             .align = XENT_FLEX_ALIGN_STRETCH
    },
	  xtk_kids(
		xtk_text(ui, xtk_fmt(ui, "Note %d", index), (XtkTextDesc) {.weight = XTK_FONT_SEMI_BOLD}),
		xtk_text(ui, note_text(ui, index), (XtkTextDesc) {.size = 13})
	  )
	);
}

static int32_t        kContactGroups [26];

static int32_t const *contact_groups(void) {
	for (int g = 0; g < 26; g++) kContactGroups [g] = g % 9 == 8 ? 0 : 2 + (g * 7) % 11;
	return kContactGroups;
}

static int contact_group(int index) {
	int g = 0;
	while (g < 25 && index >= kContactGroups [g]) index -= kContactGroups [g++];
	return g;
}

static XtkEl *contact_header(XtkUi *ui, void *env, int group) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "%c", 'A' + group), (XtkTextDesc) {0});
}

static char const *contact_text(char *buf, size_t size, int index) {
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	snprintf(buf, size, "%c. %s (#%d)", 'A' + contact_group(index), first, index);
	return buf;
}

static XtkEl *contact_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	char buf [48];
	return xtk_text(ui, xtk_fmt(ui, "%s", contact_text(buf, sizeof(buf), index)), (XtkTextDesc) {0});
}

static char        kContactText [256][48];
static char const *kContactNames [256];

static int         contact_names(void) {
	int32_t const *groups = contact_groups();
	int            count  = 0;
	for (int g = 0; g < 26; g++) count += groups [g];
	if (count > 256) count = 256;
	for (int i = 0; i < count; i++) kContactNames [i] = contact_text(kContactText [i], sizeof(kContactText [i]), i);
	return count;
}

XtkEl *page_listview(XtkUi *ui, Model const *m) {
	char const *status = m->list_sel >= 0 ? xtk_fmt(ui, "Selected row: %d", m->list_sel) : "Nothing selected yet.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ListView",
		  "A virtualized list: 100,000 rows, but only the viewport's worth of controls exist. "
		  "Scrolling recycles "
		  "rows in place."
		),
		example(
		  ui, "Click a row to select it.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_list(
				  ui,
				  (XtkListDesc) {
					.nitem  = LIST_ROWS,
					.itemh  = 40.0f,
					.item   = list_row,
					.sel    = m->list_sel,
					.select = xtk_msg(MSG_LIST_SELECT),
				  }
				),
				NAN, 420.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13})
			)
		  )
		),
		example(
		  ui,
		  "SelectionMode Extended: Ctrl+click toggles, Shift+click ranges, Ctrl+A selects all. Drag from the empty "
		  "area "
		  "below the rows to box-select; Ctrl inverts, Shift extends.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_list(
				  ui,
				  (XtkListDesc) {
					.nitem       = 12,
					.itemh       = 40.0f,
					.item        = list_row,
					.sel_mode    = XTK_LIST_SELECT_EXTENDED,
					.rect_select = true,
					.select      = xtk_msg(MSG_MULTI_SELECT),
				  }
				),
				NAN, 600.0f
			  ),
			  xtk_text(
				ui,
				m->multi_lead >= 0 ? xtk_fmt(ui, "Lead selection: row %d", m->multi_lead)
				                   : "Selection is control-retained; the lead index posts here.",
				(XtkTextDesc) {.size = 13}
			  )
			)
		  )
		),
		example(
		  ui, "Variable-height rows: 10,000 notes measured as they are realized; resize the window to rewrap.",
		  xtk_sized(xtk_list(ui, (XtkListDesc) {.nitem = 10000, .item = note_row, .sel = -1}), NAN, 360.0f)
		),
		example(
		  ui, "Grouped with sticky headers: the current group's header stays pinned until the next one pushes it out.",
		  xtk_sized(
			xtk_list(
			  ui,
			  (XtkListDesc) {
				.item         = contact_row,
				.sel          = -1,
				.groups       = contact_groups(),
				.ngroup       = 26,
				.group_header = contact_header,
				.nname        = contact_names(),
				.names        = kContactNames,
			  }
			),
			NAN, 360.0f
		  )
		)
	  )
	);
}
