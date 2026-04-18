/**
 * @file flux_dmanip_sync.c
 * @brief DirectManipulation viewport synchronization for ScrollView nodes.
 */
#include "flux_dmanip_sync.h"
#include "fluxent/flux_component_data.h"

#include <windows.h>

/* Callback invoked by DManip when scroll offset changes */
static void dmanip_on_update(void *ctx, float sx, float sy) {
	FluxScrollData *sd = ( FluxScrollData * ) ctx;
	if (!sd) return;
	sd->scroll_x = sx;
	sd->scroll_y = sy;
}

/* Recursive implementation */
static void sync_node(
  FluxDManip *dmanip, XentContext *ctx, FluxNodeStore *store, XentNodeId node, float ax_dip, float ay_dip, float scale
) {
	if (node == XENT_NODE_INVALID) return;

	XentRect lr = {0};
	xent_get_layout_rect(ctx, node, &lr);
	float node_ax  = ax_dip + lr.x;
	float node_ay  = ay_dip + lr.y;

	float child_ax = node_ax;
	float child_ay = node_ay;

		if (xent_get_control_type(ctx, node) == XENT_CONTROL_SCROLL) {
			FluxNodeData   *nd = flux_node_store_get(store, node);
			FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
				if (sd) {
						/* Lazily create the viewport on first visit. */
						if (!sd->dmanip_viewport) {
							FluxDManipViewport *vp = NULL;
								if (SUCCEEDED(flux_dmanip_viewport_create(dmanip, &vp))) {
									flux_dmanip_viewport_set_callback(vp, dmanip_on_update, sd);
									sd->dmanip_viewport = vp;
								}
						}
						if (sd->dmanip_viewport) {
							FluxDManipViewport *vp = ( FluxDManipViewport * ) sd->dmanip_viewport;

							/* DPI first — the conversions in sync_transform and the
							 * OnContentUpdated callback read this. */
							flux_dmanip_viewport_set_dpi_scale(vp, scale);

							int px = ( int ) (node_ax * scale + 0.5f);
							int py = ( int ) (node_ay * scale + 0.5f);
							int pw = ( int ) (lr.width * scale + 0.5f);
							int ph = ( int ) (lr.height * scale + 0.5f);
							flux_dmanip_viewport_set_rect(vp, px, py, pw, ph);

							int cw = ( int ) (sd->content_w * scale + 0.5f);
							int ch = ( int ) (sd->content_h * scale + 0.5f);
							/* Content must be at least as large as the viewport or
							 * DManip refuses to pan on that axis. */
							if (cw < pw) cw = pw;
							if (ch < ph) ch = ph;
							flux_dmanip_viewport_set_content_size(vp, cw, ch);

							/* Keep DManip's transform in sync with our scroll state
							 * while idle.  This is a no-op during active manipulation
							 * (DManip is the source of truth then) but catches wheel
							 * and scrollbar-drag offset changes so the next touch
							 * gesture starts from the right origin. */
							flux_dmanip_viewport_sync_transform(vp, sd->scroll_x, sd->scroll_y);
						}

					/* Children see the scrolled content, but for DManip-viewport
					 * geometry purposes this node's own rect is what matters. */
					child_ax -= sd->scroll_x;
					child_ay -= sd->scroll_y;
				}
		}

	XentNodeId c = xent_get_first_child(ctx, node);
		while (c != XENT_NODE_INVALID) {
			sync_node(dmanip, ctx, store, c, child_ax, child_ay, scale);
			c = xent_get_next_sibling(ctx, c);
		}
}

void flux_dmanip_sync_tree(FluxDManip *dmanip, XentContext *ctx, FluxNodeStore *store, XentNodeId root, float scale) {
	if (!dmanip || !ctx || !store || root == XENT_NODE_INVALID) return;
	sync_node(dmanip, ctx, store, root, 0.0f, 0.0f, scale);
}

/* Recursive cleanup */
static void cleanup_node(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;

		if (xent_get_control_type(ctx, node) == XENT_CONTROL_SCROLL) {
			FluxNodeData *nd = flux_node_store_get(store, node);
				if (nd && nd->component_data) {
					FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
						if (sd->dmanip_viewport) {
							flux_dmanip_viewport_destroy(( FluxDManipViewport * ) sd->dmanip_viewport);
							sd->dmanip_viewport = NULL;
						}
				}
		}

	XentNodeId c = xent_get_first_child(ctx, node);
		while (c != XENT_NODE_INVALID) {
			cleanup_node(ctx, store, c);
			c = xent_get_next_sibling(ctx, c);
		}
}

void flux_dmanip_cleanup_tree(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return;
	cleanup_node(ctx, store, root);
}
