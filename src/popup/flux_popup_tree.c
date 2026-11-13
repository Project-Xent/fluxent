#include "popup/flux_popup_tree.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_control_registry.h"
#include "runtime/flux_time.h"
#include "store/flux_node_data.h"

#include <dwmapi.h>
#include <stdlib.h>

struct FluxPopupTree {
	FluxWindow          *owner;
	XentCtx             *ctx;
	FluxStore           *store;
	XentNodeId           root;
	FluxEngine          *engine;
	FluxControlRegistry *registry;
	FluxRenderCache     *cache;
	FluxText            *text;
	FluxTheme           *theme;
	int64_t              last_frame;
};

void flux_popup_tree_destroy(FluxPopupTree *tree) {
	if (!tree) return;
	flux_engine_destroy(tree->engine);
	flux_control_registry_destroy(tree->registry);
	flux_render_cache_destroy(tree->cache);
	flux_text_destroy(tree->text);
	flux_theme_destroy(tree->theme);
	free(tree);
}

FluxPopupTree *flux_popup_tree_create(FluxWindow *owner, XentCtx *ctx, FluxStore *store, XentNodeId root) {
	FluxPopupTree *tree = calloc(1, sizeof(*tree));
	if (!tree) return NULL;
	tree->owner    = owner;
	tree->ctx      = ctx;
	tree->store    = store;
	tree->root     = root;
	tree->engine   = flux_engine_create(store);
	tree->registry = flux_control_registry_create();
	tree->cache    = flux_render_cache_create(1024);
	tree->text     = flux_text_create();
	tree->theme    = flux_theme_create();
	if (!tree->engine || !tree->registry || !tree->cache || !tree->text || !tree->theme) {
		flux_popup_tree_destroy(tree);
		return NULL;
	}
	flux_register_builtins(tree->registry);
	flux_engine_bind(tree->engine, tree->registry, NULL);
	return tree;
}

static bool popup_tree_sync(FluxPopupTree *tree, XentNodeId node) {
	FluxNodeData *nd   = flux_store_find(tree->store, node);
	bool          busy = nd && nd->component_type == FLUX_CONTROL_LOOPING_SELECTOR && flux_looping_sync(node, nd);
	for (XentNodeId child = xent_node_first(tree->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(tree->ctx, child))
		busy |= popup_tree_sync(tree, child);
	return busy;
}

static FluxThemeColors popup_tree_colors(FluxPopupTree *tree, FluxPopup *popup) {
	BOOL dark = flux_theme_systemdark();
	DwmGetWindowAttribute(flux_window_hwnd(tree->owner), DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	flux_theme_setmode(tree->theme, dark ? FLUX_THEME_DARK : FLUX_THEME_LIGHT);
	FluxThemeColors colors   = *flux_theme_colors(tree->theme);
	colors.flyout_background = flux_popup_acrylictint(popup, colors.flyout_background);
	return colors;
}

bool flux_popup_tree_paint(FluxPopupTree *tree, FluxPopup *popup, FluxRenderCtx const *surface) {
	if (!tree || !xent_node_valid(tree->ctx, tree->root)) return false;
	XentRect box = {0};
	if (!xent_layout_rect(tree->ctx, tree->root, &box)) return false;
	xent_layout(tree->ctx, tree->root, box.w, box.h);
	bool syncing = popup_tree_sync(tree, tree->root);
	xent_layout(tree->ctx, tree->root, box.w, box.h);
	flux_engine_collect(tree->engine, tree->ctx, tree->root);

	int64_t         now    = flux_perf_now();
	FluxThemeColors colors = popup_tree_colors(tree, popup);
	FluxRenderCtx   rc     = *surface;
	rc.text                = tree->text;
	rc.cache               = tree->cache;
	rc.theme               = &colors;
	rc.dpi                 = flux_window_dpi(tree->owner);
	rc.now                 = flux_perf_seconds(now);
	rc.dt                  = flux_compute_dt(tree->last_frame, now);
	rc.is_dark             = flux_theme_mode(tree->theme) == FLUX_THEME_DARK;
	bool animating         = false;
	rc.animations_active   = &animating;
	tree->last_frame       = now;
	flux_rcache_begin(tree->cache);
	flux_engine_execute(tree->engine, &rc);
	return animating || syncing;
}
