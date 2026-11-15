#include "gallery.h"

#define DET_CELL_PAD 8.0f

typedef struct DetailsEnv {
	Model const *m;
	int          rows [DET_ROWS];
} DetailsEnv;

static XtkEl *details_column(XtkUi *ui, Model const *m, int col) {
	XtkSortDirection sort = m->det_sort != col ? XTK_SORT_NONE : m->det_desc ? XTK_SORT_DESCENDING : XTK_SORT_ASCENDING;
	return xtk_colitem(
	  ui, kDetHeaders [col],
	  (XtkColItemDesc) {
		.width   = m->det_width [col],
		.sort    = sort,
		.click   = xtk_msg_i(MSG_DET_SORT, col),
		.resize  = xtk_msg_i(MSG_DET_RESIZE, col),
		.fit     = xtk_msg_i(MSG_DET_FIT, col),
		.reorder = (XtkMsg) {.tag = MSG_DET_REORDER, .d = col},
	}
	);
}

static XtkEl *details_header(XtkUi *ui, Model const *m) {
	XtkEl **kids = xtk_children_alloc(ui, DET_COLS + 1);
	for (int i = 0; i < DET_COLS; i++) kids [i] = details_column(ui, m, m->det_order [i]);
	kids [DET_COLS] = XTK_END;
	return xtk_colheader(ui, kids);
}

static XtkEl *details_row(XtkUi *ui, void *env, int index) {
	DetailsEnv const *e    = ( DetailsEnv const * ) env;
	XtkEl           **kids = xtk_children_alloc(ui, DET_COLS + 1);
	for (int i = 0; i < DET_COLS; i++) {
		int    col  = e->m->det_order [i];
		XtkEl *cell = xtk_text(
		  ui, gallery_det_cell(e->rows [index], col),
		  (XtkTextDesc) {.size = col ? 12.0f : 14.0f, .nowrap = true, .trim = XTK_TRIM_CHAR_ELLIPSIS}
		);
		kids [i] = xtk_margin(
		  xtk_sized(cell, e->m->det_width [col] - DET_CELL_PAD, NAN), (XentInsets) {DET_CELL_PAD, 0, 0, 0}
		);
	}
	kids [DET_COLS] = XTK_END;
	return xtk_row(ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER}, kids);
}

XtkEl *page_details(XtkUi *ui, Model const *m) {
	static DetailsEnv details;
	DetailsEnv       *env = &details;
	env->m                = m;
	gallery_det_rows(m, env->rows);
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH
    },
	  xtk_kids(
		page_header(ui, "Details header", "File Explorer's column header over a list, as Files builds it."),
		example(
		  ui, "Click a header to sort, drag a divider to resize, double-click it to fit, drag a header to reorder.",
		  xtk_sized(
			xtk_list(
			  ui,
			  (XtkListDesc) {
				.nitem = DET_ROWS,
				.itemh = 36.0f,
				.item  = details_row,
				.env   = env,
				.header
	            = xtk_row(ui, (XtkStackDesc) {.padding = {16.0f, 0, 12.0f, 0}}, xtk_kids(details_header(ui, m))),
			  }
			),
			NAN, 420.0f
		  )
		)
	  )
	);
}
