#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_compose_anim.h"
#include "store/flux_node_data.h"
#include "compose/flux_compose_render.h"
#include "compose/flux_effect.h"
#include "compose/flux_shape.h"
#include "compose/flux_surface.h"
#include "compose/flux_visual_tree.h"
#include "compose/flux_interaction.h"

#include "controls/data/flux_scroll_data.h"
#include "fluxent/flux_control_type.h"
#include "render/flux_engine.h"
#include "fluxent/flux_node_store.h"
#include "render/flux_render_snapshot.h"
#include "render/flux_display_paint.h"
#include "render/flux_render_internal.h"

#include <xtk/xtk_backend.h>
#include <cwinrt/cast.h>
#include <cd2d.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_ACRYLIC_BLUR_RADIUS    30.0f
#define FLUX_ACRYLIC_TINT_OPACITY   0.72f

#define FLUX_COMPOSE_INLINE_ACRYLIC 0

typedef struct ContentNode {
	XentNodeId                 owner;
	WUC_Sprite                *sprite;
	WUC_Visual                *visual;
	FluxSurface               *surface;
	FluxShapeRect             *acrylic;
	FluxShapeRect             *fill;
	WUC_CompositionColorBrush *fill_brush;
	FluxColor                  fill_color;
	bool                       fill_set;
	FluxInteraction           *tracker;
	bool                       scroll_bound;
	int                        w;
	int                        h;
	uint32_t                   seen;
	bool                       in_use;
} ContentNode;

struct FluxComposeRender {
	FluxCompositor                *c;
	WUC_Comp                      *comp;
	WUC_CompositionGraphicsDevice *gdev;
	FluxControlRegistry const     *registry;
	XtkBackend                    *backend;
	FluxVisualTree                *vt;
	FluxAnimKit                   *anim;
	ContentNode                   *nodes;
	uint32_t                       cap;
	uint32_t                       generation;
};

FluxComposeRender *flux_compose_render_create(
  FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root, FluxControlRegistry const *registry,
  XtkBackend *backend
) {
	if (!c || !gdev || !root) return NULL;
	FluxComposeRender *r = ( FluxComposeRender * ) calloc(1, sizeof(*r));
	if (!r) return NULL;
	r->c        = c;
	r->comp     = flux_compositor_comp(c);
	r->gdev     = gdev;
	r->registry = registry;
	r->backend  = backend;
	r->vt       = flux_visual_tree_create(c, root);
	if (!r->vt) {
		free(r);
		return NULL;
	}
	r->anim = flux_anim_kit_create(c);
	return r;
}

static void content_release(ContentNode *cn) {
	if (cn->tracker) flux_interaction_destroy(cn->tracker);
	cn->tracker      = NULL;
	cn->scroll_bound = false;
	if (cn->visual) (( IUnknown * ) cn->visual)->lpVtbl->Release(( IUnknown * ) cn->visual);
	if (cn->sprite) (( IUnknown * ) cn->sprite)->lpVtbl->Release(( IUnknown * ) cn->sprite);
	if (cn->fill_brush) (( IUnknown * ) cn->fill_brush)->lpVtbl->Release(( IUnknown * ) cn->fill_brush);
	flux_surface_destroy(cn->surface);
	flux_shape_rect_destroy(cn->acrylic);
	flux_shape_rect_destroy(cn->fill);
	cn->visual     = NULL;
	cn->sprite     = NULL;
	cn->fill       = NULL;
	cn->fill_brush = NULL;
	cn->surface    = NULL;
	cn->acrylic    = NULL;
	cn->fill_set   = false;
	cn->in_use     = false;
	cn->owner      = XENT_NODE_INVALID;
}

void flux_compose_render_destroy(FluxComposeRender *r) {
	if (!r) return;
	for (uint32_t i = 0; i < r->cap; i++)
		if (r->nodes [i].in_use) content_release(&r->nodes [i]);
	free(r->nodes);
	flux_anim_kit_destroy(r->anim);
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

static void content_attach(WUC_Container *container, WUC_Visual *sprite_visual) {
	WUC_VisualCollection *children = NULL;
	if (FAILED(wuc_container_get__children(container, &children))) return;
	( void ) wuc_visual_collection_insert_at_bottom(children, sprite_visual);
	(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
}

static bool
content_create(FluxComposeRender *r, ContentNode *cn, XentNodeId node, WUC_Container *container, int w, int h) {
	cn->surface = flux_surface_create(r->c, r->gdev, w, h);
	if (!cn->surface) return false;
	if (FAILED(wuc_comp_create_sprite_visual(r->comp, &cn->sprite))) {
		flux_surface_destroy(cn->surface);
		cn->surface = NULL;
		return false;
	}
	cn->visual = flux_compose_as_visual(cn->sprite);
	if (!cn->visual) {
		(( IUnknown * ) cn->sprite)->lpVtbl->Release(( IUnknown * ) cn->sprite);
		cn->sprite = NULL;
		flux_surface_destroy(cn->surface);
		cn->surface = NULL;
		return false;
	}
	WUC_Brush *brush = NULL;
	if (SUCCEEDED(cwinrt_query(flux_surface_brush(cn->surface), &CWINRT_IID_WUC_ICompositionBrush, ( void ** ) &brush)))
	{
		( void ) wuc_sprite_put__brush(cn->sprite, brush);
		(( IUnknown * ) brush)->lpVtbl->Release(( IUnknown * ) brush);
	}
	content_attach(container, cn->visual);
	cn->owner  = node;
	cn->in_use = true;
	return true;
}

static bool content_ensure(FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h) {
	ContentNode *cn = content_slot(r, node);
	if (!cn) return false;
	if (cn->in_use && cn->w == w && cn->h == h) return true;
	if (!cn->in_use && !content_create(r, cn, node, container, w, h)) return false;
	if (cn->in_use && (cn->w != w || cn->h != h)) ( void ) flux_surface_resize(cn->surface, w, h);

	WFN_Vector_2 size = {( float ) w, ( float ) h};
	( void ) wuc_visual_put__size(cn->visual, size);
	cn->w = w;
	cn->h = h;
	return true;
}

static bool node_is_acrylic(FluxStore *store, XentNodeId node) {
	switch (flux_node_type(store, node)) {
	case FLUX_CONTROL_CARD        :
	case FLUX_CONTROL_FLYOUT      :
	case FLUX_CONTROL_MENU_FLYOUT :
	case FLUX_CONTROL_TOOLTIP     : return true;
	default                       : return false;
	}
}

static FluxColor acrylic_tint(FluxColor fill) {
	uint32_t  a = ( uint32_t ) ((fill.rgba & 0xffu) * FLUX_ACRYLIC_TINT_OPACITY + 0.5f);
	FluxColor t = {(fill.rgba & 0xffffff00u) | (a & 0xffu)};
	return t;
}

static void
acrylic_ensure(FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h, float radius) {
	ContentNode *cn = content_owned(r, node);
	if (!cn) return;
	if (cn->acrylic) {
		flux_shape_setsize(cn->acrylic, ( float ) w, ( float ) h);
		flux_shape_setradius(cn->acrylic, radius);
		return;
	}

	cn->acrylic = flux_shape_rect_create(r->c, ( float ) w, ( float ) h, radius);
	if (!cn->acrylic) return;

	WUC_Brush *blur = flux_effect_backdrop_blur(r->c, FLUX_ACRYLIC_BLUR_RADIUS);
	if (blur) {
		flux_shape_setfillbrush(cn->acrylic, blur);
		(( IUnknown * ) blur)->lpVtbl->Release(( IUnknown * ) blur);
	}

	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(container, &children))) {
		( void ) wuc_visual_collection_insert_at_bottom(children, flux_shape_rect_visual(cn->acrylic));
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
}

static void acrylic_release(FluxComposeRender *r, XentNodeId node) {
	ContentNode *cn = content_owned(r, node);
	if (!cn || !cn->acrylic) return;
	flux_shape_rect_destroy(cn->acrylic);
	cn->acrylic = NULL;
}

static FluxControlState content_state(XentCtx *ctx, XentNodeId node, FluxNodeData const *nd) {
	FluxControlState s = {0};
	s.enabled          = xent_sem_enabled(ctx, node);
	if (!nd) return s;
	s.hovered      = nd->state.hovered;
	s.pressed      = nd->state.pressed;
	s.focused      = nd->state.focused && !nd->state.suppress_focus;
	s.pointer_type = nd->state.pointer_type;
	return s;
}

static void content_paint(
  FluxComposeRender *r, FluxStore *store, XentNodeId node, XentRect const *rect, FluxRenderCtx const *rc_tmpl,
  FluxNodeData const *nd, FluxRenderSnap *snap, FluxFillSink *out_sink
) {
	ContentNode *cn = content_owned(r, node);
	if (!cn || !cn->surface) return;
	POINT               offset;
	ID2D1DeviceContext *dc = flux_surface_begin(cn->surface, &offset);
	if (!dc) return;

	float dpi_x = rc_tmpl->dpi.dpi_x > 0.0f ? rc_tmpl->dpi.dpi_x : FLUX_DPI_BASE;
	float dpi_y = rc_tmpl->dpi.dpi_y > 0.0f ? rc_tmpl->dpi.dpi_y : FLUX_DPI_BASE;
	float scale = dpi_x / FLUX_DPI_BASE;
	ID2D1DeviceContext_SetDpi(dc, dpi_x, dpi_y);

	ID2D1SolidColorBrush *brush = NULL;
	D2D1_COLOR_F          black = {0, 0, 0, 1};
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) dc, &black, NULL, &brush);

	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) dc, NULL);

	FluxRenderCtx rc        = *rc_tmpl;
	rc.d2d                  = dc;
	rc.brush                = brush;
	rc.fill_sink            = out_sink;

	FluxControlState state  = content_state(flux_store_ctx(store), node, nd);
	FluxRect         bounds = {offset.x / scale, offset.y / scale, rect->w, rect->h};

	flux_display_paint_main(store, r->backend, r->registry, &rc, snap, &bounds, &state);
	flux_display_paint_overlay(r->backend, r->registry, &rc, snap, &bounds, &state);

	if (brush) ID2D1SolidColorBrush_Release(brush);
	flux_surface_end(cn->surface);
}

static WUI_Color content_wui_color(FluxColor c) {
	WUI_Color out;
	out.A = ( uint8_t ) (c.rgba & 0xffu);
	out.R = ( uint8_t ) ((c.rgba >> 24) & 0xffu);
	out.G = ( uint8_t ) ((c.rgba >> 16) & 0xffu);
	out.B = ( uint8_t ) ((c.rgba >> 8) & 0xffu);
	return out;
}

static void
content_fill_ensure(FluxComposeRender *r, ContentNode *cn, WUC_Container *container, int w, int h, float radius) {
	if (cn->fill) return;

	cn->fill = flux_shape_rect_create(r->c, ( float ) w, ( float ) h, radius);
	if (!cn->fill) return;
	if (SUCCEEDED(wuc_comp_create_color_brush(r->comp, &cn->fill_brush)))
		flux_shape_setfillbrush(cn->fill, ( WUC_Brush * ) cn->fill_brush);

	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(container, &children))) {
		( void ) wuc_visual_collection_insert_at_bottom(children, flux_shape_rect_visual(cn->fill));
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
}

static void content_fill_apply(
  FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h, FluxFillSink const *sink
) {
	ContentNode *cn = content_owned(r, node);
	if (!cn || !sink->written) return;

	content_fill_ensure(r, cn, container, w, h, sink->corner_radius);
	if (!cn->fill || !cn->fill_brush) return;

	flux_shape_setsize(cn->fill, ( float ) w, ( float ) h);
	flux_shape_setradius(cn->fill, sink->corner_radius);

	if (cn->fill_set && cn->fill_color.rgba == sink->color.rgba) return;
	cn->fill_color = sink->color;
	cn->fill_set   = true;

	if (r->anim && SUCCEEDED(flux_anim_color(r->anim, cn->fill_brush, L"Color", sink->color, 0.15, FLUX_EASE_STANDARD)))
		return;
	( void ) wuc_composition_color_brush_put__color(cn->fill_brush, content_wui_color(sink->color));
}

static void content_bind_scroll_tracker(FluxComposeRender *r, XentNodeId node, WUC_Visual *holder) {
	ContentNode *cn = content_owned(r, node);
	if (!cn || cn->tracker) return;

	WUC_Container *container = flux_vtree_visual(r->vt, node);
	WUC_Visual    *cv        = container ? flux_compose_as_visual(container) : NULL;
	if (cv) {
		cn->tracker = flux_interaction_create(r->c, cv);
		(( IUnknown * ) cv)->lpVtbl->Release(( IUnknown * ) cv);
	}
	if (cn->tracker && SUCCEEDED(flux_interaction_bind_offset(cn->tracker, holder))) {
		cn->scroll_bound = true;
		flux_vtree_bindtracker(r->vt, node, true);
	}
}

static bool
content_scroll_poll(ContentNode *cn, FluxScrollData *sd, float scale, bool *anim_active, float *out_px, float *out_py) {
	*out_px = 0.0f;
	*out_py = 0.0f;
	if (!flux_interaction_poll(cn->tracker, out_px, out_py)) return false;
	sd->scroll_x = *out_px / scale;
	sd->scroll_y = *out_py / scale;
	if (anim_active) *anim_active = true;
	return true;
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
	if (content_scroll_poll(cn, sd, scale, anim_active, &px, &py)) return;
	content_scroll_push(cn, sd, scale, px, py);
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
	if (!container || w < 1 || h < 1 || !render_reserve(r, node) || !content_ensure(r, node, container, w, h)) return;

	FluxNodeData const *nd = flux_store_find(store, node);
	FluxRenderSnap      snap;
	memset(&snap, 0, sizeof(snap));
	flux_snapshot_build(&snap, ctx, node, nd);

	if (FLUX_COMPOSE_INLINE_ACRYLIC && node_is_acrylic(store, node)) {
		acrylic_ensure(r, node, container, w, h, snap.corner_radius * scale);
		snap.background = acrylic_tint(snap.background);
	}
	else { acrylic_release(r, node); }

	FluxFillSink sink = {0};
	content_paint(r, store, node, &rect, rc_tmpl, nd, &snap, &sink);
	sink.corner_radius *= scale;
	content_fill_apply(r, node, container, w, h, &sink);
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
  FluxComposeRender *r, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxRenderCtx const *rc_tmpl
) {
	if (!r || !ctx || root == XENT_NODE_INVALID) return;

	float scale = rc_tmpl && rc_tmpl->dpi.dpi_x > 0.0f ? rc_tmpl->dpi.dpi_x / FLUX_DPI_BASE : 1.0f;

	flux_visual_tree_reconcile(r->vt, ctx, store, root, scale);
	r->generation++;

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
	content_sweep(r);
}
