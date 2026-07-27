#include "app.h"
#include <math.h>
#include <stdio.h>

static void on_combo(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static XentNodeId split_pane_item(GalleryApp *g, XentNodeId parent, char const *icon, char const *label) {
	return g_button(g, parent, label, icon, XTK_BUTTON_SUBTLE, true);
}

void page_split_view(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_split_view_sync);
	static char const *const kSplitModes [] = {"Inline", "CompactInline", "Overlay", "CompactOverlay"};
	Model const             *m              = &g->model;
	g_page_header(
	  g, parent, "SplitView",
	  "A collapsible side pane beside main content: Inline and CompactInline push the content; Overlay and "
	  "CompactOverlay float the pane over it."
	);
	{
		XentNodeId controls = g_wraprow(g, parent, 12.0f);
		XentNodeId mode     = flux_combo_init(&(FluxComboBoxDesc) {
		  .ctx            = gallery_ctx(g),
		  .store          = gallery_store(g),
		  .parent         = controls,
		  .window         = flux_app_window(g->flux),
		  .text           = flux_app_text(g->flux),
		  .theme          = flux_app_theme(g->flux),
		  .items          = kSplitModes,
		  .item_count     = 4,
		  .selected_index = m->split_mode,
		  .placeholder    = "Display mode",
		  .on_select      = on_combo,
		  .userdata       = gallery_bind(g, MSG_SPLIT_MODE),
		});
		( void ) mode;
		XentNodeId place = g_checkbox(g, controls, "Pane on right", m->split_right, true);
		g_on_change_bool(g, place, MSG_SPLIT_PLACE);
		g->live_control [MSG_SPLIT_MODE]  = mode;
		g->live_control [MSG_SPLIT_PLACE] = place;
	}
	{
		XentNodeId split = flux_splitview_init(&(FluxSplitViewDesc) {
		  .ctx            = gallery_ctx(g),
		  .store          = gallery_store(g),
		  .parent         = parent,
		  .display_mode   = m->split_mode,
		  .pane_placement = m->split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT,
		  .pane_open      = m->split_open,
		});
		xent_seth(gallery_ctx(g), split, 320.0f);
		XentNodeId content_wrap = flux_splitbody_init(gallery_ctx(g), gallery_store(g), split);
		XentNodeId pane_wrap    = flux_splitpane_init(gallery_ctx(g), gallery_store(g), split);
		XentNodeId pane         = g_col(g, pane_wrap, 4.0f, 8.0f, 8.0f, 8.0f, 8.0f);
		split_pane_item(g, pane, "Home", "Home");
		split_pane_item(g, pane, "Folder", "Documents");
		split_pane_item(g, pane, "Pictures", "Pictures");
		split_pane_item(g, pane, "Settings", "Settings");
		XentNodeId content = g_col(g, content_wrap, 12.0f, 16.0f, 16.0f, 16.0f, 16.0f);
		XentNodeId toggle  = g_button(g, content, "\xE2\x98\xB0 Toggle pane", NULL, 0, true);
		g_on_invoke(g, toggle, MSG_SPLIT_TOGGLE);
		g_text(g, content, "Main content area", 20.0f, FLUX_FONT_SEMI_BOLD);
		g->live_control [MSG_SPLIT_TOGGLE] = split;
		g->live_output [MSG_SPLIT_TOGGLE]  = g_text(
		  g, content, m->split_open ? "The pane is open." : "The pane is collapsed.", 13.0f, FLUX_FONT_REGULAR
		);
	}
}

void page_split_view_sync(GalleryApp *g) {
	FluxStore *store = gallery_store(g);
	XentNodeId split = g->live_control [MSG_SPLIT_TOGGLE];
	flux_splitview_configure(
	  store, split, g->model.split_mode, g->model.split_right ? XTK_SPLITVIEW_RIGHT : XTK_SPLITVIEW_LEFT, 0.0f, 0.0f
	);
	flux_splitview_setopen(store, split, g->model.split_open);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_SPLIT_TOGGLE],
	  g->model.split_open ? "The pane is open." : "The pane is collapsed."
	);
}
