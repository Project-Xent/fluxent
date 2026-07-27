#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "virtual.h"

static void on_tree_select(void *ud, int flat, bool selected) {
	if (!selected) return;
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = flat});
}

static void on_tree_expand(void *ud, int flat, bool expanded) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = expanded ? flat : -1 - flat});
}

static void tree_add_nodes(GalleryApp *g, XentNodeId tree) {
	FluxStore *store = gallery_store(g);
	int        docs  = flux_tree_add_node(store, tree, -1, "Documents", "Folder");
	if (docs >= 0) {
		flux_tree_set_expanded(store, tree, docs, true);
		flux_tree_add_node(store, tree, docs, "Proposal.docx", NULL);
		flux_tree_add_node(store, tree, docs, "Budget.xlsx", NULL);
	}
	int pics = flux_tree_add_node(store, tree, -1, "Pictures", "Pictures");
	if (pics >= 0) {
		flux_tree_add_node(store, tree, pics, "Vacation", NULL);
		flux_tree_add_node(store, tree, pics, "Screenshots", NULL);
	}
	flux_tree_add_node(store, tree, -1, "Music", "MusicNote");
}

void page_tree_view(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_tree_view_sync);
	Model const *m   = &g->model;
	char const  *sel = m->tree_sel >= 0 ? gallery_fmt(g, "Selected flat row: %d", m->tree_sel) : "Nothing selected.";
	char const  *inv
	  = m->tree_invoke >= 0 ? gallery_fmt(g, "Last invoked: %d", m->tree_invoke) : "Double-click a row to invoke.";
	g_page_header(
	  g, parent, "TreeView",
	  "Hierarchical data flattened into a virtualized list. Chevron toggles expand; row selects and invokes."
	);
	{
		XentNodeId   ex          = g_example(g, parent, "Sample folder tree.");
		XentNodeId   col         = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		GalleryBind *select_bind = gallery_bind(g, MSG_TREE_SELECT);
		GalleryBind *invoke_bind = gallery_bind(g, MSG_TREE_INVOKE);
		GalleryBind *expand_bind = gallery_bind(g, MSG_TREE_EXPAND);
		XentNodeId   tree        = flux_tree_init(&(FluxTreeViewDesc) {
		  .ctx        = gallery_ctx(g),
		  .store      = gallery_store(g),
		  .parent     = col,
		  .window     = flux_app_window(g->flux),
		  .input      = flux_app_input(g->flux),
		  .row_height = 28.0f,
		  .on_select  = on_tree_select,
		  .select_ctx = select_bind,
		  .on_invoke  = gallery_list_invoke,
		  .invoke_ctx = invoke_bind,
		  .on_expand  = on_tree_expand,
		  .expand_ctx = expand_bind,
		});
		xent_seth(gallery_ctx(g), tree, 280.0f);
		tree_add_nodes(g, tree);
		g->live_control [MSG_TREE_SELECT] = tree;
		g->live_output [MSG_TREE_SELECT]  = g_text(g, col, sel, 13.0f, FLUX_FONT_REGULAR);
		g->live_output [MSG_TREE_INVOKE]  = g_text(g, col, inv, 13.0f, FLUX_FONT_REGULAR);
	}
}

void page_tree_view_sync(GalleryApp *g) {
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_TREE_SELECT],
	  g->model.tree_sel >= 0 ? gallery_fmt(g, "Selected flat row: %d", g->model.tree_sel) : "Nothing selected."
	);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_TREE_INVOKE],
	  g->model.tree_invoke >= 0 ? gallery_fmt(g, "Last invoked: %d", g->model.tree_invoke)
								: "Double-click a row to invoke."
	);
}
