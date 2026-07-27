#include "app.h"
#include <math.h>
#include <stdio.h>
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

typedef struct TabDemo {
	GalleryApp *g;
	XentNodeId  view;
	int         slot_id [FLUX_TAB_VIEW_MAX_TABS];
	bool        ignore_select;
} TabDemo;

static int tab_model_index(TabDemo const *demo, int slot) {
	if (( unsigned ) slot >= FLUX_TAB_VIEW_MAX_TABS) return -1;
	int id = demo->slot_id [slot];
	for (int i = 0; i < demo->g->model.ntab; i++)
		if (demo->g->model.tab_ids [i] == id) return i;
	return -1;
}

static void on_tab_select(void *ctx, int slot) {
	TabDemo *demo = ctx;
	if (demo->ignore_select) {
		demo->ignore_select = false;
		return;
	}
	int index = tab_model_index(demo, slot);
	if (index >= 0) gallery_dispatch(demo->g, (XtkMsg) {.tag = MSG_TAB_SELECT, .i = index});
}

static bool on_tab_close_request(void *ctx, int slot) {
	( void ) ctx;
	( void ) slot;
	return true;
}

static void on_tab_closed(void *ctx, int slot) {
	TabDemo *demo  = ctx;
	int      index = tab_model_index(demo, slot);
	if (index < 0) return;
	demo->ignore_select  = demo->g->model.tab_sel == index;
	demo->slot_id [slot] = -1;
	gallery_dispatch(demo->g, (XtkMsg) {.tag = MSG_TAB_CLOSE, .i = index});
}

static void on_tab_add(void *ctx) {
	TabDemo *demo = ctx;
	gallery_dispatch(demo->g, (XtkMsg) {.tag = MSG_TAB_ADD});
}

static int tab_find_slot(TabDemo const *demo, int id) {
	for (int i = 0; i < FLUX_TAB_VIEW_MAX_TABS; i++)
		if (demo->slot_id [i] == id) return i;
	return -1;
}

static void tab_fill_content(TabDemo *demo, XentNodeId content, int id) {
	GalleryApp *g   = demo->g;
	XentNodeId  col = g_col(g, content, 8.0f, 16.0f, 16.0f, 16.0f, 16.0f);
	g_text(g, col, gallery_fmt(g, "Document %d", id), 20.0f, FLUX_FONT_SEMI_BOLD);
	g_text(
	  g, col, "This page is a function of the model's selection — the same view subtree follows it.", 13.0f,
	  FLUX_FONT_REGULAR
	);
}

static void tab_add_model_item(TabDemo *demo, int id) {
	XentNodeId content = XENT_NODE_INVALID;
	int        slot    = flux_tabs_add_tab(
	  gallery_store(demo->g), demo->view, "OpenFile", gallery_fmt(demo->g, "Document %d", id), &content
	);
	if (slot < 0) return;
	demo->slot_id [slot] = id;
	if (content != XENT_NODE_INVALID) tab_fill_content(demo, content, id);
}

void page_tabview(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_tabview_sync);
	Model const *m     = &g->model;
	int          count = m->ntab;
	int          sel   = (m->tab_sel >= 0 && m->tab_sel < count) ? m->tab_sel : 0;
	g_page_header(
	  g, parent, "TabView", "Tabs live in the model: selection, close, and add update the retained control."
	);
	{
		XentNodeId ex   = g_example(g, parent, "A TabView with closable tabs and an add button.");
		TabDemo   *demo = gallery_page_alloc(g, sizeof(*demo));
		if (!demo) return;
		( void ) gallery_bind(g, MSG_TAB_SELECT);
		( void ) gallery_bind(g, MSG_TAB_CLOSE);
		( void ) gallery_bind(g, MSG_TAB_ADD);
		demo->g = g;
		for (int i = 0; i < FLUX_TAB_VIEW_MAX_TABS; i++) demo->slot_id [i] = -1;
		XentNodeId tv                 = flux_tabs_init(&(FluxTabViewDesc) {
		  .ctx                  = gallery_ctx(g),
		  .store                = gallery_store(g),
		  .parent               = ex,
		  .window               = flux_app_window(g->flux),
		  .text                 = flux_app_text(g->flux),
		  .input                = flux_app_input(g->flux),
		  .on_selection_changed = on_tab_select,
		  .selection_ctx        = demo,
		  .on_close_requested   = on_tab_close_request,
		  .close_requested_ctx  = demo,
		  .on_tab_closed        = on_tab_closed,
		  .tab_closed_ctx       = demo,
		  .on_add_tab           = on_tab_add,
		  .add_tab_ctx          = demo,
		});
		demo->view                    = tv;
		g->live_data [MSG_TAB_SELECT] = demo;
		xent_seth(gallery_ctx(g), tv, 280.0f);
		for (int i = 0; i < count; i++) tab_add_model_item(demo, m->tab_ids [i]);
		int slot = count ? tab_find_slot(demo, m->tab_ids [sel]) : -1;
		if (slot >= 0) flux_tabs_select(gallery_store(g), tv, slot);
	}
}

void page_tabview_sync(GalleryApp *g) {
	TabDemo *demo = g->live_data [MSG_TAB_SELECT];
	if (!demo) return;
	for (int i = 0; i < g->model.ntab; i++)
		if (tab_find_slot(demo, g->model.tab_ids [i]) < 0) tab_add_model_item(demo, g->model.tab_ids [i]);
	int sel = g->model.tab_sel;
	if (sel < 0 || sel >= g->model.ntab) return;
	int slot = tab_find_slot(demo, g->model.tab_ids [sel]);
	if (slot >= 0) flux_tabs_select(gallery_store(g), demo->view, slot);
}
