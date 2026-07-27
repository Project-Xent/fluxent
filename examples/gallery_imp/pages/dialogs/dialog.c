#include "app.h"
#include <math.h>
#include <stdio.h>
#include "host/flux_prop_apply.h"
#include "render/flux_icon.h"

static void on_dialog_result(void *ud, FluxDialogResult result) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = ( int ) result});
}

void page_dialog(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_dialog_sync);
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	Model const             *m           = &g->model;
	char const *result = m->dialog_result >= 0 && m->dialog_result <= 2 ? kResults [m->dialog_result] : "(none yet)";
	g_page_header(g, parent, "ContentDialog", "A modal dialog controlled by the model: open while a flag is true.");
	{
		XentNodeId  ex   = g_example(g, parent, "A dialog with Delete / Cancel buttons.");
		GalleryDemo demo = g_demo_card(g, ex);
		XentNodeId  row  = demo.items;
		XentNodeId  btn  = g_button(g, row, "Show dialog", NULL, 0, true);
		g_on_invoke(g, btn, MSG_DIALOG_OPEN);
		g->live_output [MSG_DIALOG_RESULT] = g_output(g, demo.row, gallery_fmt(g, "Last result: %s", result));
	}
	{
		XentNodeId content = XENT_NODE_INVALID;
		XentNodeId dlg     = flux_dialog_init(&(FluxContentDialogDesc) {
		  .ctx            = gallery_ctx(g),
		  .store          = gallery_store(g),
		  .overlay_parent = flux_app_overlay(g->flux),
		  .input          = flux_app_input(g->flux),
		  .window         = flux_app_window(g->flux),
		  .theme          = flux_app_theme(g->flux),
		  .title          = "Delete file?",
		  .primary_text   = "Delete",
		  .close_text     = "Cancel",
		  .width          = 400.0f,
		  .content_height = 56.0f,
		  .on_result      = on_dialog_result,
		  .userdata       = gallery_bind(g, MSG_DIALOG_RESULT),
		  .out_content    = &content,
		});
		flux_dialog_sync_host(gallery_store(g), dlg, m->dialog_open);
		g->live_control [MSG_DIALOG_OPEN] = dlg;
		if (content != XENT_NODE_INVALID)
			g_text(
			  g, content, "This file will be permanently removed. This cannot be undone.", 14.0f, FLUX_FONT_REGULAR
			);
	}
}

void page_dialog_sync(GalleryApp *g) {
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	char const              *result
	  = g->model.dialog_result >= 0 && g->model.dialog_result <= 2 ? kResults [g->model.dialog_result] : "(none yet)";
	flux_dialog_sync_host(gallery_store(g), g->live_control [MSG_DIALOG_OPEN], g->model.dialog_open);
	xent_settext(gallery_ctx(g), g->live_output [MSG_DIALOG_RESULT], gallery_fmt(g, "Last result: %s", result));
}
