#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "store/flux_node_data.h"
#include "compose/flux_compose_render.h"
#include "compose/flux_surface.h"
#include "render/flux_engine.h"
#include "compose/flux_visual_tree.h"
#include "compose/flux_interaction.h"

#include "controls/data/flux_scroll_data.h"
#include "controls/factory/flux_init_container.h"
#include "fluxent/flux_control_type.h"
#include "render/flux_engine.h"
#include "fluxent/flux_node_store.h"
#include "render/flux_display_paint.h"
#include "render/flux_render_internal.h"

#include <xtk/xtk_backend.h>
#include <cwinrt/cast.h>
#include <cd2d.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ContentNode {
	XentNodeId       owner;
	FluxInteraction *tracker;
	bool             scroll_bound;
	uint32_t         seen;
	bool             in_use;
} ContentNode;

struct FluxComposeRender {
	FluxCompositor                *c;
	WUC_Comp                      *comp;
	WUC_CompositionGraphicsDevice *gdev;
	FluxEngine                    *engine;
	FluxVisualTree                *vt;
	WUC_Visual                    *root;
	ContentNode                   *nodes;
	uint32_t                       cap;
	uint32_t                       generation;

	/* One drawing surface for the whole window, painted in absolute coordinates once per frame.
	 * Per-node surfaces put every control in a shared atlas whose origin the compositor relocates
	 * at will (observed: up to 16 different origins for one surface, differing by a pixel), so any
	 * repaint could land the same pixels one pixel off. A single surface has one origin per frame,
	 * which is what makes the Direct2D path stable. */
	FluxSurface                   *frame;
	WUC_Sprite                    *frame_sprite;
	WUC_Visual                    *frame_visual;
	ID2D1DeviceContext            *frame_dc;
	ID2D1SolidColorBrush          *frame_brush;
	FluxRenderCtx                  frame_rc;
	int                            frame_w;
	int                            frame_h;
};

FluxComposeRender *flux_compose_render_create(
  FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root, FluxEngine *engine
) {
	if (!c || !gdev || !root) return NULL;
	FluxComposeRender *r = ( FluxComposeRender * ) calloc(1, sizeof(*r));
	if (!r) return NULL;
	r->c      = c;
	r->comp   = flux_compositor_comp(c);
	r->gdev   = gdev;
	r->engine = engine;
	r->root   = root;
	r->vt     = flux_visual_tree_create(c, root);
	if (!r->vt) {
		free(r);
		return NULL;
	}
	return r;
}

static void frame_attach(WUC_Container *container, WUC_Visual *visual) {
	WUC_VisualCollection *children = NULL;
	if (FAILED(wuc_container_get__children(container, &children))) return;
	/* On top: the frame surface carries every node's content, so it has to sit above the
	 * per-node decorative visuals (fills, acrylic). Inserting it at the bottom hid the whole
	 * interface behind those opaque layers. */
	( void ) wuc_visual_collection_insert_at_top(children, visual);
	(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
}

/* Make sure the single frame surface matches the window size and attach its brush to a sprite
 * under the root visual. */
static bool frame_ensure(FluxComposeRender *r, int w, int h) {
	WUC_Visual *root = r->root;
	if (!r->frame) {
		r->frame = flux_surface_create(r->c, r->gdev, w, h);
		if (!r->frame) return false;
		if (FAILED(wuc_comp_create_sprite_visual(r->comp, &r->frame_sprite))) return false;
		r->frame_visual = flux_compose_as_visual(r->frame_sprite);
		if (!r->frame_visual) return false;
		WUC_CompositionSurfaceBrush *surface_brush = flux_surface_brush(r->frame);
		if (surface_brush) {
			WUC_Brush *brush = NULL;
			if (SUCCEEDED(cwinrt_query(surface_brush, &CWINRT_IID_WUC_ICompositionBrush, ( void ** ) &brush))) {
				( void ) wuc_sprite_put__brush(r->frame_sprite, brush);
				(( IUnknown * ) brush)->lpVtbl->Release(( IUnknown * ) brush);
			}
		}
		/* A Visual is not a Container: query the container interface first, otherwise the call
		 * lands on the wrong vtable slot. */
		WUC_Container *container = NULL;
		if (SUCCEEDED(cwinrt_query(root, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &container))) {
			frame_attach(container, r->frame_visual);
			(( IUnknown * ) container)->lpVtbl->Release(( IUnknown * ) container);
		}
		r->frame_w = w;
		r->frame_h = h;
	}
	else if (r->frame_w != w || r->frame_h != h) {
		if (FAILED(flux_surface_resize(r->frame, w, h))) return false;
		r->frame_w = w;
		r->frame_h = h;
	}

	WFN_Vector_2 size = {( float ) w, ( float ) h};
	( void ) wuc_visual_put__size(r->frame_visual, size);
	return true;
}

static void content_release(ContentNode *cn) {
	if (cn->tracker) flux_interaction_destroy(cn->tracker);
	cn->tracker      = NULL;
	cn->scroll_bound = false;
	cn->in_use       = false;
	cn->owner        = XENT_NODE_INVALID;
}

void flux_compose_render_destroy(FluxComposeRender *r) {
	if (!r) return;
	for (uint32_t i = 0; i < r->cap; i++)
		if (r->nodes [i].in_use) content_release(&r->nodes [i]);
	free(r->nodes);
	if (r->frame_brush) ID2D1SolidColorBrush_Release(r->frame_brush);
	if (r->frame_visual) (( IUnknown * ) r->frame_visual)->lpVtbl->Release(( IUnknown * ) r->frame_visual);
	if (r->frame_sprite) (( IUnknown * ) r->frame_sprite)->lpVtbl->Release(( IUnknown * ) r->frame_sprite);
	flux_surface_destroy(r->frame);
	flux_visual_tree_destroy(r->vt);
	free(r);
}

static bool render_reserve(FluxComposeRender *r, XentNodeId node) {
	uint32_t idx = xent_node_index(node);
	if (idx < r->cap) return true;
	uint32_t new_cap = r->cap ? r->cap * 2 : 64;
	while (new_cap <= idx) {
		if (new_cap > UINT32_MAX / 2u) return false;
		new_cap *= 2;
	}
	ContentNode *n = ( ContentNode * ) realloc(r->nodes, ( size_t ) new_cap * sizeof(*n));
	if (!n) return false;
	memset(n + r->cap, 0, ( size_t ) (new_cap - r->cap) * sizeof(*n));
	r->nodes = n;
	r->cap   = new_cap;
	return true;
}

static ContentNode *content_slot(FluxComposeRender *r, XentNodeId node) {
	uint32_t idx = xent_node_index(node);
	if (!r || node == XENT_NODE_INVALID || idx == 0u) return NULL;
	if (!render_reserve(r, node)) return NULL;
	ContentNode *cn = &r->nodes [idx];
	if (cn->in_use && cn->owner != node) content_release(cn);
	return cn;
}

static ContentNode *content_owned(FluxComposeRender *r, XentNodeId node) {
	uint32_t idx = xent_node_index(node);
	if (!r || idx >= r->cap) return NULL;
	ContentNode *cn = &r->nodes [idx];
	return (cn->in_use && cn->owner == node) ? cn : NULL;
}

/* A node owns no visual of its own any more - its pixels live in the single frame surface. The
 * slot only tracks lifetime, so scroll trackers and fills are released when the node disappears. */
static bool content_ensure(FluxComposeRender *r, XentNodeId node) {
	ContentNode *cn = content_slot(r, node);
	if (!cn) return false;
	cn->owner  = node;
	cn->in_use = true;
	return true;
}

static void content_create_tracker(FluxComposeRender *r, ContentNode *cn, XentNodeId node) {
	WUC_Container *container = flux_vtree_visual(r->vt, node);
	WUC_Visual    *cv        = container ? flux_compose_as_visual(container) : NULL;
	if (!cv) return;
	cn->tracker = flux_interaction_create(r->c, cv);
	(( IUnknown * ) cv)->lpVtbl->Release(( IUnknown * ) cv);
}

static void content_bind_scroll_tracker(FluxComposeRender *r, XentNodeId node, WUC_Visual *holder) {
	ContentNode *cn = content_owned(r, node);
	if (!cn) return;
	if (!cn->tracker) content_create_tracker(r, cn, node);
	if (!cn->tracker || flux_vtree_trackerbound(r->vt, node)) return;
	HRESULT hr       = flux_interaction_bind_offset(cn->tracker, holder);
	cn->scroll_bound = SUCCEEDED(hr);
	flux_vtree_bindtracker(r->vt, node, cn->scroll_bound);
}

static bool
content_scroll_poll(ContentNode *cn, FluxScrollData *sd, float scale, bool *anim_active, float *out_px, float *out_py) {
	*out_px     = 0.0f;
	*out_py     = 0.0f;
	bool moved  = flux_interaction_poll(cn->tracker, out_px, out_py);
	bool active = flux_interaction_active(cn->tracker);
	if (active && anim_active) *anim_active = true;
	if (moved) {
		sd->scroll_x   = *out_px / scale;
		sd->scroll_y   = *out_py / scale;
		sd->user_moved = 1;
	}
	return active;
}

static void content_scroll_push(ContentNode *cn, FluxScrollData const *sd, float scale, float px, float py) {
	float tx = sd->scroll_x * scale;
	float ty = sd->scroll_y * scale;
	if (tx != px || ty != py) flux_interaction_scroll_to(cn->tracker, tx, ty);
}

static void content_scroll_sync(
  FluxComposeRender *r, FluxStore *store, XentNodeId node, XentRect const *rect, float scale, bool *anim_active
) {
	ContentNode *cn     = content_owned(r, node);
	WUC_Visual  *holder = flux_vtree_scrollholder(r->vt, node);
	if (!cn || !holder) return;

	FluxNodeData   *nd = flux_store_find(store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd || sd->virtualized) return;

	content_bind_scroll_tracker(r, node, holder);
	if (!cn->scroll_bound) return;

	flux_interaction_set_extent(cn->tracker, (sd->content_w - rect->w) * scale, (sd->content_h - rect->h) * scale);
	float px = 0.0f, py = 0.0f;
	bool  active = content_scroll_poll(cn, sd, scale, anim_active, &px, &py);
	flux_interaction_set_manual(cn->tracker, !active, sd->scroll_x * scale, sd->scroll_y * scale);
	if (!active) content_scroll_push(cn, sd, scale, px, py);
}

bool flux_compose_redirect_scroll(FluxComposeRender *r, XentNodeId node, uint32_t pointer_id) {
	ContentNode *cn = content_owned(r, node);
	if (!cn || !cn->tracker) return false;
	return SUCCEEDED(flux_interaction_redirectpid(cn->tracker, pointer_id));
}

static void content_sweep(FluxComposeRender *r) {
	for (uint32_t i = 0; i < r->cap; i++)
		if (r->nodes [i].in_use && r->nodes [i].seen != r->generation) content_release(&r->nodes [i]);
}

static void compose_render_node(
  FluxComposeRender *r, XentCtx *ctx, FluxStore *store, XentNodeId node, float scale, FluxRenderCtx const *rc_tmpl
) {
	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);

	int            w         = ( int ) ceilf(rect.w * scale);
	int            h         = ( int ) ceilf(rect.h * scale);
	WUC_Container *container = flux_vtree_visual(r->vt, node);
	if (!container || w < 1 || h < 1 || !render_reserve(r, node) || !content_ensure(r, node)) return;

	if (flux_node_type(store, node) == FLUX_CONTROL_SCROLL)
		content_scroll_sync(r, store, node, &rect, scale, rc_tmpl->animations_active);
	ContentNode *cn = content_owned(r, node);
	if (cn) cn->seen = r->generation;
}

static bool compose_stack_push(XentNodeId **stack, uint32_t *top, uint32_t *cap, XentNodeId node) {
	if (*top == *cap) {
		uint32_t    ncap = *cap * 2;
		XentNodeId *ns   = ( XentNodeId * ) realloc(*stack, sizeof(**stack) * ncap);
		if (!ns) return false;
		*stack = ns;
		*cap   = ncap;
	}
	(*stack) [(*top)++] = node;
	return true;
}

static bool compose_push_children(XentNodeId **stack, uint32_t *top, uint32_t *cap, XentCtx *ctx, XentNodeId parent) {
	for (XentNodeId child = xent_node_first(ctx, parent); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ctx, child))
		if (!compose_stack_push(stack, top, cap, child)) return false;
	return true;
}

void flux_compose_render_frame(
  FluxComposeRender *r, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxRenderCtx const *rc_tmpl, FluxColor clear
) {
	if (!r || !ctx || root == XENT_NODE_INVALID) return;

	float scale = rc_tmpl && rc_tmpl->dpi.dpi_x > 0.0f ? rc_tmpl->dpi.dpi_x / FLUX_DPI_BASE : 1.0f;

	flux_visual_tree_reconcile(r->vt, ctx, store, root, scale);
	r->generation++;

	XentRect root_rect = {0};
	xent_layout_rect(ctx, root, &root_rect);
	int fw = ( int ) ceilf(root_rect.w * scale);
	int fh = ( int ) ceilf(root_rect.h * scale);
	if (fw < 1 || fh < 1) return;

	if (!frame_ensure(r, fw, fh)) return;

	POINT offset = {0, 0};
	if (!(r->frame_dc = flux_surface_begin(r->frame, &offset))) return;

	float dpi_x = rc_tmpl && rc_tmpl->dpi.dpi_x > 0.0f ? rc_tmpl->dpi.dpi_x : FLUX_DPI_BASE;
	float dpi_y = rc_tmpl && rc_tmpl->dpi.dpi_y > 0.0f ? rc_tmpl->dpi.dpi_y : FLUX_DPI_BASE;
	ID2D1DeviceContext_SetDpi(r->frame_dc, dpi_x, dpi_y);

	/* One origin for the whole frame, applied once. Every node then draws in absolute DIPs. */
	D2D1_MATRIX_3X2_F base = {1.0f, 0.0f, 0.0f, 1.0f, offset.x / scale, offset.y / scale};
	ID2D1RenderTarget_SetTransform(( ID2D1RenderTarget * ) r->frame_dc, &base);
	/* Same rule as the Direct2D path (app_clear_color): transparent when a DWM material is active,
	 * the theme's solid background otherwise. Clearing to transparent unconditionally made the whole
	 * window see-through once the backdrop was set to none. */
	D2D1_COLOR_F bg = {flux_color_rf(clear), flux_color_gf(clear), flux_color_bf(clear), flux_color_af(clear)};
	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) r->frame_dc, &bg);

	if (!r->frame_brush) {
		D2D1_COLOR_F black = {0, 0, 0, 1};
		ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) r->frame_dc, &black, NULL, &r->frame_brush);
	}
	if (rc_tmpl) {
		r->frame_rc       = *rc_tmpl;
		r->frame_rc.d2d   = r->frame_dc;
		r->frame_rc.brush = r->frame_brush;
	}

	/* Pixels come from the engine's command list, exactly as on the Direct2D path: it carries the
	 * per-node transform pushes, scroll clips and child clipping. Painting nodes here by hand lost
	 * all of that, which is why scrolling did not move and flyouts overlapped. */
	flux_engine_collect(r->engine, ctx, root);
	flux_engine_execute(r->engine, &r->frame_rc);

	uint32_t    cap   = 64;
	uint32_t    top   = 0;
	XentNodeId *stack = ( XentNodeId * ) malloc(sizeof(*stack) * cap);
	if (!stack) return;
	stack [top++] = root;

	while (top > 0) {
		XentNodeId node = stack [--top];
		compose_render_node(r, ctx, store, node, scale, rc_tmpl);
		if (!compose_push_children(&stack, &top, &cap, ctx, node)) goto done;
	}

done:
	free(stack);
	if (r->frame_dc) {
		flux_surface_end(r->frame);
		r->frame_dc = NULL;
	}
	content_sweep(r);
}
