#include "app.h"

#include <stdlib.h>

static void on_nav_select(void *ctx, int index) {
	GalleryApp *g = ( GalleryApp * ) ctx;
	gallery_dispatch(g, (XtkMsg) {.tag = MSG_NAV, .i = index});
}

static int nav_add_child(FluxStore *store, XentNodeId nav, XtkNavItemDesc const *it, int parent) {
	if (parent < 0) return -1;
	return flux_nav_addchild(store, nav, parent, it->icon, it->label);
}

static int nav_add_one(FluxStore *store, XentNodeId nav, XtkNavItemDesc const *it, int *parent) {
	switch (it->kind) {
	case XTK_NAV_ITEM      : *parent = flux_nav_add_item(store, nav, it->icon, it->label); return *parent;
	case XTK_NAV_HEADER    : return flux_nav_add_header(store, nav, it->label);
	case XTK_NAV_SEPARATOR : return flux_nav_add_separator(store, nav);
	case XTK_NAV_FOOTER    : return flux_nav_addfooter(store, nav, it->icon, it->label);
	case XTK_NAV_CHILD     : return nav_add_child(store, nav, it, *parent);
	}
	return -1;
}

bool gallery_shell_build(GalleryApp *g) {
	XentNodeId content = XENT_NODE_INVALID;
	g->nav             = flux_nav_init(&(FluxNavViewDesc) {
	  .ctx                  = gallery_ctx(g),
	  .store                = gallery_store(g),
	  .parent               = flux_app_root(g->flux),
	  .window               = flux_app_window(g->flux),
	  .text                 = flux_app_text(g->flux),
	  .theme                = flux_app_theme(g->flux),
	  .adaptive             = true,
	  .window_title_bar     = true,
	  .app_title            = "FluXent Gallery",
	  .on_selection_changed = on_nav_select,
	  .userdata             = g,
	  .out_content          = &content,
	});
	if (g->nav == XENT_NODE_INVALID) return false;

	int parent = -1;
	for (int i = 0; i < PAGE_COUNT; i++) nav_add_one(gallery_store(g), g->nav, &kNavItems [i], &parent);
	flux_nav_select(gallery_store(g), g->nav, g->model.page);

	g->content_host = content != XENT_NODE_INVALID ? content : flux_nav_content_node(gallery_store(g), g->nav);
	return g->content_host != XENT_NODE_INVALID;
}
