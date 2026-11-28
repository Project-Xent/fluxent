#include "app.h"

#include <string.h>

static char const *kind_label(Model const *m) { return m->dd_files ? "Files:" : "Text:"; }

static void        drag_drop_sync(GalleryApp *g) {
	Model const *m = &g->model;
	gallery_settext(g, g->live_output [MSG_DD_HOVER], gallery_dd_prompt(m));
	gallery_settext(g, g->live_output [MSG_DD_TEXT], kind_label(m));
	gallery_settext(g, g->live_output [MSG_DD_FILES], gallery_dd_result(m));
	gallery_settext(g, g->live_output [MSG_DD_DONE], gallery_dd_completed(m));
}

static void drop_target(PageBuild *b, XtkNodeRef parent) {
	Model const *m      = &b->app->model;
	XtkLocalId   target = xtk_container_create(
	  b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_DROP_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.drop  = XTK_DROP_COPY,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(target);
	xtk_container_ondraghover(b->txn, ref, page_post, page_bind(b, MSG_DD_HOVER));
	xtk_container_ondropfiles(b->txn, ref, page_post, page_bind(b, MSG_DD_FILES));
	xtk_container_ondroptext(b->txn, ref, page_post, page_bind(b, MSG_DD_TEXT));
	XtkNodeRef card = xtk_ref_local(ui_card(b->txn, ref, 12.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f));
	page_keepout(b, MSG_DD_HOVER, ui_text(b->txn, card, gallery_dd_prompt(m), 14.0f, FLUX_FONT_SEMI_BOLD));
	page_keepout(b, MSG_DD_TEXT, ui_text(b->txn, card, kind_label(m), 12.0f, FLUX_FONT_REGULAR));
	page_keepout(b, MSG_DD_FILES, ui_text(b->txn, card, gallery_dd_result(m), 14.0f, FLUX_FONT_REGULAR));
}

static void source(PageBuild *b, XtkNodeRef parent, char const *label, char const *text, char const *files) {
	XtkLocalId local = xtk_container_create(
	  b->txn, parent,
	  &(XtkContainerProps) {
		.set           = XTK_CONTAINER_DIR_MASK
	                   | (text ? XTK_CONTAINER_DRAGTEXT_MASK : 0u)
	                   | (files ? XTK_CONTAINER_DRAGFILES_MASK : 0u),
		.dir           = XTK_STACK_COLUMN,
		.dragtext      = text,
		.dragtext_len  = text ? ( uint32_t ) strlen(text) : 0u,
		.dragfiles     = files,
		.dragfiles_len = files ? ( uint32_t ) strlen(files) : 0u,
	  }
	);
	xtk_container_ondragcompleted(b->txn, xtk_ref_local(local), page_post, page_bind(b, MSG_DD_DONE));
	XtkNodeRef card = xtk_ref_local(ui_card(b->txn, xtk_ref_local(local), 0.0f, XENT_FLEX_ALIGN_START, 16.0f));
	( void ) ui_text(b->txn, card, label, 14.0f, FLUX_FONT_REGULAR);
}

bool page_drag_drop_build(PageBuild *b) {
	page_sync(b, drag_drop_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "Drag and drop", "OLE drag and drop: drop targets, drag sources, and the system drag image."
	);
	drop_target(b, page_example(b, parent, "A drop target that copies files or text."));
	XtkNodeRef sources = page_example(b, parent, "Drag sources. The file card offers this gallery's executable.");
	XtkNodeRef row     = xtk_ref_local(ui_row(b->txn, sources, 12.0f, false));
	source(b, row, "Drag this text", "Hello from Xent", NULL);
	source(b, row, "Drag this program file", NULL, b->app->model.dd_exe);
	page_keepout(b, MSG_DD_DONE, page_note(b, sources, gallery_dd_completed(&b->app->model)));
	return true;
}
