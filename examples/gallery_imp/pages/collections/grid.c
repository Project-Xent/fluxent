#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "virtual.h"

static void fill_grid_cell(GalleryApp *g, XentNodeId item, int index) {
	XentNodeId col = g_col(g, item, 0.0f, 0, 0, 0, 0);
	xent_setitems(gallery_ctx(g), col, XENT_FLEX_ALIGN_CENTER);
	xent_setjustify(gallery_ctx(g), col, XENT_FLEX_JUSTIFY_CENTER);
	g_text(g, col, gallery_fmt(g, "%d", index), 20.0f, FLUX_FONT_SEMI_BOLD);
	g_text(g, col, gallery_fmt(g, "Item %d", index), 11.0f, FLUX_FONT_REGULAR);
}

void page_gridview(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_gridview_sync);
	Model const *m      = &g->model;
	char const  *status = m->grid_sel >= 0 ? gallery_fmt(g, "Selected cell: %d", m->grid_sel) : "Nothing selected yet.";
	g_page_header(
	  g, parent, "GridView",
	  "The same virtualized items host wrapping cells across auto-fit columns. "
	  "Arrow keys navigate in 2D; the selection ring is the WinUI accent border."
	);
	{
		XentNodeId ex  = g_example(g, parent, "10,000 uniform cells; columns follow the width.");
		XentNodeId col = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		gallery_list_create(
		  g, col, XTK_LIST_KIND_GRID, 10000, 104.0f, 152.0f, 0, m->grid_sel, XTK_LIST_SELECT_SINGLE, MSG_GRID_SELECT,
		  fill_grid_cell, 420.0f
		);
		g->live_output [MSG_GRID_SELECT] = g_text(g, col, status, 13.0f, FLUX_FONT_REGULAR);
	}
}

void page_gridview_sync(GalleryApp *g) {
	flux_list_set_selected(gallery_store(g), g->live_control [MSG_GRID_SELECT], g->model.grid_sel);
	gallery_list_realize(g->live_data [MSG_GRID_SELECT]);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_GRID_SELECT],
	  g->model.grid_sel >= 0 ? gallery_fmt(g, "Selected cell: %d", g->model.grid_sel) : "Nothing selected yet."
	);
}
