#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "virtual.h"

static char const *const kFonts [] = {
  "Arial", "Calibri", "Cambria", "Consolas", "Courier New", "Georgia", "Segoe UI", "Tahoma", "Times New Roman",
  "Verdana"};

#define FONT_COUNT ( int ) (sizeof(kFonts) / sizeof(kFonts [0]))

static void fill_listbox_row(GalleryApp *g, XentNodeId item, int index) {
	g_text(g, item, kFonts [index], 14.0f, FLUX_FONT_REGULAR);
}

void page_listbox(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_listbox_sync);
	Model const *m      = &g->model;
	char const  *status = m->listbox_sel >= 0 ? gallery_fmt(g, "Font: %s", kFonts [m->listbox_sel]) : "Pick a font.";
	g_page_header(g, parent, "ListBox", "The bordered list surface with accent-filled selection.");
	{
		XentNodeId ex
		  = g_example(g, parent, "Selection fills the row with the accent at WinUI's 0.6/0.8/0.9 opacities.");
		XentNodeId row  = g_wraprow(g, ex, 16.0f);
		XentNodeId list = gallery_list_create(
		  g, row, XTK_LIST_KIND_LIST_BOX, FONT_COUNT, 40.0f, 0.0f, 1, m->listbox_sel, XTK_LIST_SELECT_SINGLE,
		  MSG_LISTBOX_SELECT, fill_listbox_row, 320.0f
		);
		xent_setw(gallery_ctx(g), list, 260.0f);
		g->live_output [MSG_LISTBOX_SELECT] = g_text(g, row, status, 13.0f, FLUX_FONT_REGULAR);
	}
}

void page_listbox_sync(GalleryApp *g) {
	flux_list_set_selected(gallery_store(g), g->live_control [MSG_LISTBOX_SELECT], g->model.listbox_sel);
	gallery_list_realize(g->live_data [MSG_LISTBOX_SELECT]);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_LISTBOX_SELECT],
	  g->model.listbox_sel >= 0 ? gallery_fmt(g, "Font: %s", kFonts [g->model.listbox_sel]) : "Pick a font."
	);
}
