#include "app.h"

static XtkTreeNodeDesc const kDocs []     = {{.text = "Proposal.docx"}, {.text = "Budget.xlsx"}};
static XtkTreeNodeDesc const kPictures [] = {{.text = "Vacation"}, {.text = "Screenshots"}};

static char const           *select_text(GalleryApp *g) {
	return g->model.tree_sel >= 0 ? gallery_fmt(g, "Selected flat row: %d", g->model.tree_sel) : "Nothing selected.";
}

static char const *invoke_text(GalleryApp *g) {
	return g->model.tree_invoke >= 0 ? gallery_fmt(g, "Last invoked: %d", g->model.tree_invoke)
	                                 : "Double-click a row to invoke.";
}

static void tree_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_TREE_SELECT], select_text(g));
	gallery_settext(g, g->live_output [MSG_TREE_INVOKE], invoke_text(g));
}

bool page_tree_view_build(PageBuild *b) {
	page_sync(b, tree_sync);
	Model const          *m         = &b->app->model;
	XtkTreeNodeDesc const roots [3] = {
	  {.text = "Documents", .icon = "Folder", .expanded = m->tree_docs_expanded, .children = kDocs, .child_count = 2},
	  {.text         = "Pictures",
	   .icon        = "Pictures",
	   .expanded    = m->tree_pictures_expanded,
	   .children    = kPictures,
	   .child_count = 2},
	  {.text = "Music", .icon = "MusicNote"},
	};
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "TreeView",
	  "Hierarchical data flattened into a virtualized list. Chevron toggles expand; row selects and invokes."
	);
	XtkNodeRef col = xtk_ref_local(
	  ui_column(&b->txn, page_example(b, parent, "Sample folder tree."), 12.0f, XENT_FLEX_ALIGN_STRETCH)
	);
	XtkLocalId tree = xtk_tree_create(
	  &b->txn, col,
	  &(XtkTreeProps) {
		.set   = XTK_TREE_H_MASK | XTK_TREE_SEL_MASK | XTK_TREE_ITEMH_MASK,
		.h     = 280.0f,
		.sel   = m->tree_sel,
		.itemh = 28.0f,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(tree);
	xtk_txn_settree(&b->txn, ref, &(XtkTreeDesc) {.roots = roots, .nroot = 3, .sel = m->tree_sel});
	page_on(b, tree, XTK_EVENT_TREE_SELECT, MSG_TREE_SELECT, 0);
	page_on(b, tree, XTK_EVENT_TREE_EXPAND, MSG_TREE_EXPAND, 0);
	xtk_tree_oninvoke(&b->txn, ref, page_post, page_bind(b, MSG_TREE_INVOKE));
	page_keepout(b, MSG_TREE_SELECT, page_note(b, col, select_text(b->app)));
	page_keepout(b, MSG_TREE_INVOKE, page_note(b, col, invoke_text(b->app)));
	return true;
}
