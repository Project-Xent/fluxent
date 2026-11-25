#include "popup/flux_popup_tree.h"
#include "controls/flux_module.h"
#include "runtime/flux_time.h"
#include "store/flux_node_data.h"
#include "controls/data/flux_scroll_data.h"
#include "input/flux_input_internal.h"

#include <dwmapi.h>
#include <stdlib.h>

struct FluxPopupTree {
	FluxWindow      *owner;
	XentCtx         *ctx;
	FluxStore       *store;
	XentNodeId       root;
	FluxEngine      *engine;
	FluxRenderCache *cache;
	FluxText        *text;
	FluxTheme       *theme;
	int64_t          last_frame;
};

void flux_popup_tree_destroy(FluxPopupTree *tree) {
	if (!tree) return;
	flux_engine_destroy(tree->engine);
	flux_render_cache_destroy(tree->cache);
	flux_text_destroy(tree->text);
	flux_theme_destroy(tree->theme);
	free(tree);
}

FluxPopupTree *flux_popup_tree_create(FluxWindow *owner, XentCtx *ctx, FluxStore *store, XentNodeId root) {
	FluxPopupTree *tree = calloc(1, sizeof(*tree));
	if (!tree) return NULL;
	tree->owner  = owner;
	tree->ctx    = ctx;
	tree->store  = store;
	tree->root   = root;
	tree->engine = flux_engine_create(store);
	tree->cache  = flux_render_cache_create(1024);
	tree->text   = flux_text_create();
	tree->theme  = flux_theme_create();
	if (!tree->engine || !tree->cache || !tree->text || !tree->theme) {
		flux_popup_tree_destroy(tree);
		return NULL;
	}
	flux_engine_bind(tree->engine, NULL);
	return tree;
}

static bool popup_tree_wheeling(FluxNodeData const *nd) {
	return nd
	    && nd->component_type == FLUX_CONTROL_SCROLL
	    && nd->component_data
	    && (( FluxScrollData const * ) nd->component_data)->wheel_active;
}

static bool popup_tree_sync(FluxPopupTree *tree, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(tree->store, node);
	bool          synced
	  = nd
	 && (nd->component_type == FLUX_CONTROL_LOOPING_SELECTOR || nd->component_type == FLUX_CONTROL_SCROLL || nd->component_type == FLUX_CONTROL_PROGRESS);
	FluxControlModule const *m = synced ? flux_module_get(nd->component_type) : NULL;
	uint32_t frame             = m && m->tree && m->tree->frame ? m->tree->frame(tree->ctx, tree->store, node, nd) : 0u;
	bool     busy              = (frame & FLUX_FRAME_REDRAW) || popup_tree_wheeling(nd);
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
	FluxInput     *input = flux_store_input(tree->store);
	FluxUiaContext info  = {tree->ctx, tree->root, flux_popup_hwnd(popup), tree->store, tree->text};
	if (input) flux_access_keys_paint(&input->access_keys, &info, input, &rc);
	return animating || syncing;
}
