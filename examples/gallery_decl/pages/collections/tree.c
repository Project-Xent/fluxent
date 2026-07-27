#include "gallery.h"

static XtkTreeNodeDesc const kTreeDocsChildren [] = {
  {.text = "Proposal.docx"},
  {.text = "Budget.xlsx"},
};

static XtkTreeNodeDesc const kTreePicturesChildren [] = {
  {.text = "Vacation"},
  {.text = "Screenshots"},
};

static XtkTreeNodeDesc const kTreeRoots [] = {
  {
   .text        = "Documents",
   .icon        = "Folder",
   .expanded    = true,
   .children    = kTreeDocsChildren,
   .child_count = 2,
   },
  {
   .text        = "Pictures",
   .icon        = "Pictures",
   .children    = kTreePicturesChildren,
   .child_count = 2,
   },
  {.text = "Music", .icon = "MusicNote"},
};

XtkEl *page_tree_view(XtkUi *ui, Model const *m) {
	char const *sel = m->tree_sel >= 0 ? xtk_fmt(ui, "Selected flat row: %d", m->tree_sel) : "Nothing selected.";
	char const *inv
	  = m->tree_invoke >= 0 ? xtk_fmt(ui, "Last invoked: %d", m->tree_invoke) : "Double-click a row to invoke.";
	XtkTreeNodeDesc roots [3];
	for (int i = 0; i < 3; i++) roots [i] = kTreeRoots [i];
	roots [0].expanded = m->tree_docs_expanded;
	roots [1].expanded = m->tree_pictures_expanded;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "TreeView",
		  "Hierarchical data flattened into a virtualized list. Chevron toggles expand; row selects and invokes."
		),
		example(
		  ui, "Sample folder tree.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_tree(
				  ui,
				  (XtkTreeDesc) {
					.roots  = roots,
					.nroot  = 3,
					.sel    = m->tree_sel,
					.rowh   = 28.0f,
					.select = xtk_msg(MSG_TREE_SELECT),
					.invoke = xtk_msg(MSG_TREE_INVOKE),
					.expand = xtk_msg(MSG_TREE_EXPAND),
				  }
				),
				NAN, 280.0f
			  ),
			  xtk_text(ui, sel, (XtkTextDesc) {.size = 13}), xtk_text(ui, inv, (XtkTextDesc) {.size = 13})
			)
		  )
		)
	  )
	);
}
