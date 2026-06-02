#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_compose_anim.h"
#include "compose/flux_compose_render.h"
#include "compose/flux_effect.h"
#include "compose/flux_shape.h"
#include "compose/flux_surface.h"
#include "compose/flux_visual_tree.h"
#include "input/flux_interaction.h"

#include "fluxent/controls/flux_scroll_data.h"
#include "fluxent/flux_control_type.h"
#include "fluxent/flux_engine.h"
#include "fluxent/flux_node_store.h"
#include "fluxent/flux_render_snapshot.h"
#include "render/flux_render_internal.h"

#include <cwinrt/cast.h>
#include <cd2d.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* In-content acrylic tuning. Standard deviation of the backdrop Gaussian, and
 * the fraction of the control's own fill alpha kept as the tint over it. WinUI
 * acrylic is a translucent tint above a blurred sample of the content behind;
 * these match that look closely enough to validate on a GPU session (final
 * values are a runtime judgment). */
#define FLUX_ACRYLIC_BLUR_RADIUS    30.0f
#define FLUX_ACRYLIC_TINT_OPACITY   0.72f

/* Inline (in-window) acrylic for cards: a blurred backdrop behind the content.
 * Disabled: the backdrop-blur brush samples nothing usable in this windowed
 * composition target, so it renders opaque black over the card's fill (verified
 * on a local GPU session). Until the backdrop source is wired correctly, cards
 * fall back to the solid fill_sink path, which matches the classic D2D look. */
#define FLUX_COMPOSE_INLINE_ACRYLIC 0

typedef struct ContentNode {
	WUC_Sprite                *sprite;
	WUC_Visual                *visual;  /**< IVisual facet of sprite (sizing + tree insertion). */
	FluxSurface               *surface;
	FluxShapeRect             *acrylic; /**< Backdrop-blur material beneath content; NULL unless the node is acrylic. */
	FluxShapeRect             *fill;    /**< Rounded solid fill beneath content; its color animates off-thread. */
	WUC_CompositionColorBrush *fill_brush;
	FluxColor                  fill_color; /**< Last fill target handed to the compositor (animation de-dup). */
	bool                       fill_set;
	FluxInteraction           *tracker;      /**< Scroll node: InteractionTracker driving the content holder. */
	bool                       scroll_bound; /**< Holder Offset is bound to the tracker. */
	int                        w;
	int                        h;
	uint32_t                   seen;
	bool                       in_use;
} ContentNode;

struct FluxComposeRender {
	FluxCompositor                *c;
	WUC_Comp                      *comp;
	WUC_CompositionGraphicsDevice *gdev;
	FluxVisualTree                *vt;
	FluxAnimKit                   *anim;  /**< Off-thread keyframe/easing animations for control fills. */
	ContentNode                   *nodes; /**< Indexed by XentNodeId. */
	uint32_t                       cap;
	uint32_t                       generation;
};

FluxComposeRender *
flux_compose_render_create(FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root) {
	if (!c || !gdev || !root) return NULL;
	FluxComposeRender *r = ( FluxComposeRender * ) calloc(1, sizeof(*r));
	if (!r) return NULL;
	r->c    = c;
	r->comp = flux_compositor_comp(c);
	r->gdev = gdev;
	r->vt   = flux_visual_tree_create(c, root);
	if (!r->vt) {
		free(r);
		return NULL;
	}
	r->anim = flux_anim_kit_create(c); /* NULL-tolerant: fills then snap instead of animating. */
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
	if (node < r->cap) return true;
	uint32_t new_cap = r->cap ? r->cap * 2 : 64;
	while (new_cap <= node) new_cap *= 2;
	ContentNode *n = ( ContentNode * ) realloc(r->nodes, ( size_t ) new_cap * sizeof(*n));
	if (!n) return false;
	memset(n + r->cap, 0, ( size_t ) (new_cap - r->cap) * sizeof(*n));
	r->nodes = n;
	r->cap   = new_cap;
	return true;
}

/* Place the content sprite at the bottom of the container so child node
 * containers (added by the reconciler) compose above it. */
static void content_attach(WUC_Container *container, WUC_Visual *sprite_visual) {
	WUC_VisualCollection *children = NULL;
	if (FAILED(wuc_container_get__children(container, &children))) return;
	( void ) wuc_visual_collection_insert_at_bottom(children, sprite_visual);
	(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
}

static bool content_ensure(FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h) {
	ContentNode *cn = &r->nodes [node];
	if (cn->in_use && cn->w == w && cn->h == h) return true;

	if (!cn->in_use) {
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
		if (SUCCEEDED(
			  cwinrt_query(flux_surface_brush(cn->surface), &CWINRT_IID_WUC_ICompositionBrush, ( void ** ) &brush)
			))
		{
			( void ) wuc_sprite_put__brush(cn->sprite, brush);
			(( IUnknown * ) brush)->lpVtbl->Release(( IUnknown * ) brush);
		}
		content_attach(container, cn->visual);
		cn->in_use = true;
	}
	else if (cn->w != w || cn->h != h) { ( void ) flux_surface_resize(cn->surface, w, h); }

	WFN_Vector_2 size = {( float ) w, ( float ) h};
	( void ) wuc_visual_put__size(cn->visual, size);
	cn->w = w;
	cn->h = h;
	return true;
}

/* Controls whose background is a material, not a flat fill: cards and the popup
 * surfaces (flyout/menu/tooltip) when their content is composed inline. For
 * these the solid fill is replaced by a blurred backdrop + translucent tint. */
static bool node_is_acrylic(XentContext const *ctx, XentNodeId node) {
	switch (flux_get_control_type(ctx, node)) {
	case FLUX_CONTROL_CARD :
	case FLUX_CONTROL_FLYOUT :
	case FLUX_CONTROL_MENU_FLYOUT :
	case FLUX_CONTROL_TOOLTIP     : return true;
	default                       : return false;
	}
}

/* Scale a fill down to a tint to layer over the blurred backdrop. */
static FluxColor acrylic_tint(FluxColor fill) {
	uint32_t  a = ( uint32_t ) ((fill.rgba & 0xffu) * FLUX_ACRYLIC_TINT_OPACITY + 0.5f);
	FluxColor t = {(fill.rgba & 0xffffff00u) | (a & 0xffu)};
	return t;
}

/* Build (or update) the rounded acrylic material layer beneath the content
 * sprite. Sized to the node and clipped to its corner radius; the content
 * sprite painted on top contributes the tint + border + text. */
static void
acrylic_ensure(FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h, float radius) {
	ContentNode *cn = &r->nodes [node];
	if (cn->acrylic) {
		flux_shape_rect_set_size(cn->acrylic, ( float ) w, ( float ) h);
		flux_shape_rect_set_corner_radius(cn->acrylic, radius);
		return;
	}

	cn->acrylic = flux_shape_rect_create(r->c, ( float ) w, ( float ) h, radius);
	if (!cn->acrylic) return;

	WUC_Brush *blur = flux_effect_make_backdrop_blur(r->c, FLUX_ACRYLIC_BLUR_RADIUS);
	if (blur) {
		flux_shape_rect_set_fill_brush(cn->acrylic, blur);
		(( IUnknown * ) blur)->lpVtbl->Release(( IUnknown * ) blur);
	}

	/* Below the content sprite (itself at the bottom), above nothing. */
	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(container, &children))) {
		( void ) wuc_visual_collection_insert_at_bottom(children, flux_shape_rect_visual(cn->acrylic));
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
}

/* Drop the material layer if a node stopped being acrylic (e.g. type changed). */
static void acrylic_release(FluxComposeRender *r, XentNodeId node) {
	ContentNode *cn = &r->nodes [node];
	if (!cn->acrylic) return;
	flux_shape_rect_destroy(cn->acrylic);
	cn->acrylic = NULL;
}

static FluxControlState content_state(FluxNodeData const *nd) {
	FluxControlState s = {0};
	if (!nd) {
		s.enabled = 1;
		return s;
	}
	s.hovered      = nd->state.hovered;
	s.pressed      = nd->state.pressed;
	s.focused      = nd->state.focused;
	s.enabled      = nd->state.enabled;
	s.pointer_type = nd->state.pointer_type;
	return s;
}

static void content_paint(
  FluxComposeRender *r, FluxNodeStore *store, XentNodeId node, XentRect const *rect, FluxRenderContext const *rc_tmpl,
  FluxNodeData const *nd, FluxRenderSnapshot *snap, FluxFillSink *out_sink
) {
	ContentNode        *cn = &r->nodes [node];
	POINT               offset;
	ID2D1DeviceContext *dc = flux_surface_begin(cn->surface, &offset);
	if (!dc) return;

	/* The surface is sized in physical pixels; drive the dc at the real DPI so the
	 * renderers (which emit DIPs) rasterize crisply, matching the classic path. */
	float dpi_x = rc_tmpl->dpi.dpi_x > 0.0f ? rc_tmpl->dpi.dpi_x : FLUX_DPI_BASE;
	float dpi_y = rc_tmpl->dpi.dpi_y > 0.0f ? rc_tmpl->dpi.dpi_y : FLUX_DPI_BASE;
	float scale = dpi_x / FLUX_DPI_BASE;
	ID2D1DeviceContext_SetDpi(dc, dpi_x, dpi_y);

	ID2D1SolidColorBrush *brush = NULL;
	D2D1_COLOR_F          black = {0, 0, 0, 1};
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) dc, &black, NULL, &brush);

	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) dc, NULL); /* transparent */

	FluxRenderContext rc    = *rc_tmpl;
	rc.d2d                  = dc;
	rc.brush                = brush;
	rc.fill_sink            = out_sink;

	/* BeginDraw's offset is physical; the dc now works in DIPs, so convert. */
	FluxControlState state  = content_state(nd);
	FluxRect         bounds = {offset.x / scale, offset.y / scale, rect->w, rect->h};

	flux_engine_dispatch_render(store, &rc, snap, &bounds, &state);

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
		flux_shape_rect_set_fill_brush(cn->fill, ( WUC_Brush * ) cn->fill_brush);

	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(container, &children))) {
		( void ) wuc_visual_collection_insert_at_bottom(children, flux_shape_rect_visual(cn->fill));
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
}

/* Drive the node's solid fill on the compositor: a rounded CompositionShape whose
 * CompositionColorBrush Color animates off-thread to the control's resolved fill,
 * so hover/press color transitions run on the compositor instead of re-rasterizing
 * the D2D surface each frame. */
static void content_fill_apply(
  FluxComposeRender *r, XentNodeId node, WUC_Container *container, int w, int h, FluxFillSink const *sink
) {
	ContentNode *cn = &r->nodes [node];
	if (!sink->written) return;

	content_fill_ensure(r, cn, container, w, h, sink->corner_radius);
	if (!cn->fill || !cn->fill_brush) return;

	flux_shape_rect_set_size(cn->fill, ( float ) w, ( float ) h);
	flux_shape_rect_set_corner_radius(cn->fill, sink->corner_radius);

	if (cn->fill_set && cn->fill_color.rgba == sink->color.rgba) return;
	cn->fill_color = sink->color;
	cn->fill_set   = true;

	if (r->anim && SUCCEEDED(flux_anim_color(r->anim, cn->fill_brush, L"Color", sink->color, 0.15, FLUX_EASE_STANDARD)))
		return;
	( void ) wuc_composition_color_brush_put__color(cn->fill_brush, content_wui_color(sink->color));
}

/* Drive a scroll node's content holder from an InteractionTracker: create the
 * tracker on first sight (bound to the viewport container, offset expression-bound
 * to the holder so motion runs on the compositor), set the scrollable extent, then
 * reconcile position with FluxScrollData -- poll tracker-driven motion (touch /
 * inertia) back out, and push externally-changed offsets (wheel) into the tracker.
 * Falls back to the reconciler's static offset if the tracker cannot be created. */
static void content_scroll_sync(
  FluxComposeRender *r, FluxNodeStore *store, XentNodeId node, XentRect const *rect, float scale, bool *anim_active
) {
	ContentNode *cn     = &r->nodes [node];
	WUC_Visual  *holder = flux_visual_tree_node_scroll_holder(r->vt, node);
	if (!holder) return;

	if (!cn->tracker) {
		WUC_Container *container = flux_visual_tree_node_visual(r->vt, node);
		WUC_Visual    *cv        = container ? flux_compose_as_visual(container) : NULL;
		if (cv) {
			cn->tracker = flux_interaction_create(r->c, cv);
			(( IUnknown * ) cv)->lpVtbl->Release(( IUnknown * ) cv);
		}
		if (cn->tracker && SUCCEEDED(flux_interaction_bind_offset(cn->tracker, holder))) {
			cn->scroll_bound = true;
			flux_visual_tree_set_tracker_bound(r->vt, node, true);
		}
	}
	if (!cn->scroll_bound) return;

	FluxNodeData   *nd = flux_node_store_get(store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd) return;

	flux_interaction_set_extent(cn->tracker, (sd->content_w - rect->w) * scale, (sd->content_h - rect->h) * scale);

	float px = 0.0f, py = 0.0f;
	if (flux_interaction_poll(cn->tracker, &px, &py)) {
		sd->scroll_x = px / scale;
		sd->scroll_y = py / scale;
		if (anim_active) *anim_active = true;
		return;
	}
	float tx = sd->scroll_x * scale;
	float ty = sd->scroll_y * scale;
	if (tx != px || ty != py) flux_interaction_scroll_to(cn->tracker, tx, ty);
}

bool flux_compose_render_redirect_scroll(FluxComposeRender *r, XentNodeId node, uint32_t pointer_id) {
	if (!r || node >= r->cap || !r->nodes [node].tracker) return false;
	return SUCCEEDED(flux_interaction_redirect_pointer_id(r->nodes [node].tracker, pointer_id));
}

static void content_sweep(FluxComposeRender *r) {
	for (uint32_t i = 0; i < r->cap; i++)
		if (r->nodes [i].in_use && r->nodes [i].seen != r->generation) content_release(&r->nodes [i]);
}

void flux_compose_render_frame(
  FluxComposeRender *r, XentContext *ctx, FluxNodeStore *store, XentNodeId root, FluxRenderContext const *rc_tmpl
) {
	if (!r || !ctx || root == XENT_NODE_INVALID) return;

	/* Layout is in DIPs; the compositor is physical-pixel. Scale visual sizes,
	 * surfaces and shapes by dpi/96 so the tree matches the display (the classic
	 * path gets this for free from the D2D device-context DPI). */
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

		XentRect   rect = {0};
		xent_get_layout_rect(ctx, node, &rect);
		int            w         = ( int ) ceilf(rect.w * scale);
		int            h         = ( int ) ceilf(rect.h * scale);

		WUC_Container *container = flux_visual_tree_node_visual(r->vt, node);
		if (container && w >= 1 && h >= 1 && render_reserve(r, node) && content_ensure(r, node, container, w, h)) {
			FluxNodeData const *nd = ( FluxNodeData const * ) xent_get_userdata(ctx, node);
			FluxRenderSnapshot  snap;
			memset(&snap, 0, sizeof(snap));
			flux_snapshot_build(&snap, ctx, node, nd);

			/* Inline acrylic builds a blurred-backdrop CompositionShape; gated off
			 * (see FLUX_COMPOSE_INLINE_ACRYLIC) because the backdrop renders black
			 * in this target. With it off, the node paints its normal fill. */
			if (FLUX_COMPOSE_INLINE_ACRYLIC && node_is_acrylic(ctx, node)) {
				acrylic_ensure(r, node, container, w, h, snap.corner_radius * scale);
				snap.background = acrylic_tint(snap.background); /* tint over the blurred backdrop */
			}
			else { acrylic_release(r, node); }

			FluxFillSink sink = {0};
			content_paint(r, store, node, &rect, rc_tmpl, nd, &snap, &sink);
			sink.corner_radius *= scale; /* control wrote radius in DIPs; shape space is physical */
			content_fill_apply(r, node, container, w, h, &sink);
			if (flux_get_control_type(ctx, node) == FLUX_CONTROL_SCROLL)
				content_scroll_sync(r, store, node, &rect, scale, rc_tmpl->animations_active);
			r->nodes [node].seen = r->generation;
		}

		for (XentNodeId child = xent_get_first_child(ctx, node); child != XENT_NODE_INVALID;
		  child               = xent_get_next_sibling(ctx, child))
		{
			if (top == cap) {
				uint32_t    ncap = cap * 2;
				XentNodeId *ns   = ( XentNodeId * ) realloc(stack, sizeof(*stack) * ncap);
				if (!ns) goto done;
				stack = ns;
				cap   = ncap;
			}
			stack [top++] = child;
		}
	}

done:
	free(stack);
	content_sweep(r);
}
