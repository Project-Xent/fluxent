/* flux_dmanip.c — DirectManipulation wrapper implementation.
 * See flux_dmanip.h for the architecture overview.
 *
 * NOTE on threading: we run DManip in "host update" mode — the UI thread
 * pumps the update manager once per frame in flux_dmanip_tick().  The
 * compositor-integrated mode (zero-UI-thread scrolling via DComp) is a
 * future upgrade; right now we get WinUI-grade inertia/rails/rubber-band
 * without the complexity of per-viewport DComp visuals.
 */

#include "flux_dmanip.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <directmanipulation.h>
#include <objbase.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * IDirectManipulationViewportEventHandler — pure-C implementation.
 * We build a COM object by hand: a struct whose first field is the
 * vtable pointer, plus a static vtable.  Instances are refcounted.
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct FluxDManipHandler {
	/* MUST be first field — COM depends on this layout. */
	IDirectManipulationViewportEventHandlerVtbl *lpVtbl;
	LONG                                         ref_count;
	/* Back-pointer to the owning viewport so OnContentUpdated can invoke
	 * the user callback.  Cleared on viewport teardown so the handler
	 * doesn't dereference a freed viewport if DManip still holds a ref. */
	struct FluxDManipViewport                   *owner;
} FluxDManipHandler;

/* Forward the vtable — filled in at the bottom once all fn bodies exist. */
static IDirectManipulationViewportEventHandlerVtbl g_handler_vtbl;

static HRESULT STDMETHODCALLTYPE                   handler_QueryInterface(
  IDirectManipulationViewportEventHandler *This, REFIID riid, void **ppv
) {
	if (!ppv) return E_POINTER;
		if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectManipulationViewportEventHandler)) {
			*ppv = This;
			IDirectManipulationViewportEventHandler_AddRef(This);
			return S_OK;
		}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE handler_AddRef(IDirectManipulationViewportEventHandler *This) {
	FluxDManipHandler *self = ( FluxDManipHandler * ) This;
	return ( ULONG ) InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE handler_Release(IDirectManipulationViewportEventHandler *This) {
	FluxDManipHandler *self = ( FluxDManipHandler * ) This;
	LONG               r    = InterlockedDecrement(&self->ref_count);
	if (r == 0) free(self);
	return ( ULONG ) r;
}

/* Forward-decl of FluxDManipViewport setter — body is after the struct.
 * Lets the status handler assign without needing the full layout. */
struct FluxDManipViewport;
static void                      viewport_set_status(struct FluxDManipViewport *vp, DIRECTMANIPULATION_STATUS s);

/* Status changes — interesting for debugging but not required.
 * Possible transitions:
 *   BUILDING → ENABLED        (after viewport->Enable())
 *   ENABLED  → RUNNING        (user starts panning)
 *   RUNNING  → INERTIA        (user lifted finger, DManip extrapolates)
 *   INERTIA  → READY → ENABLED (animation settled)          */
static HRESULT STDMETHODCALLTYPE handler_OnViewportStatusChanged(
  IDirectManipulationViewportEventHandler *This, IDirectManipulationViewport *viewport,
  DIRECTMANIPULATION_STATUS current, DIRECTMANIPULATION_STATUS previous
) {
	( void ) viewport;
	( void ) previous;
	FluxDManipHandler *self = ( FluxDManipHandler * ) This;
	if (self->owner) viewport_set_status(self->owner, current);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE handler_OnViewportUpdated(
  IDirectManipulationViewportEventHandler *This, IDirectManipulationViewport *viewport
) {
	( void ) This;
	( void ) viewport;
	return S_OK;
}

/* Defined below once FluxDManipViewport is complete. */
static HRESULT STDMETHODCALLTYPE handler_OnContentUpdated(
  IDirectManipulationViewportEventHandler *This, IDirectManipulationViewport *viewport,
  IDirectManipulationContent *content
);

static FluxDManipHandler *handler_create(struct FluxDManipViewport *owner) {
	FluxDManipHandler *h = ( FluxDManipHandler * ) calloc(1, sizeof(*h));
	if (!h) return NULL;
	h->lpVtbl    = &g_handler_vtbl;
	h->ref_count = 1;
	h->owner     = owner;
	return h;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Opaque handles
 * ═══════════════════════════════════════════════════════════════════════ */

struct FluxDManip {
	HWND                              hwnd;
	IDirectManipulationManager       *manager;
	IDirectManipulationUpdateManager *update_mgr;
	bool                              activated;
	int                               live_viewports; /* rough counter */
};

struct FluxDManipViewport {
	FluxDManip                  *dm;
	IDirectManipulationViewport *viewport;
	IDirectManipulationContent  *primary; /* QI'd from viewport */
	FluxDManipHandler           *handler;
	DWORD                        handler_cookie;
	bool                         enabled;
	/* Current status, updated by OnViewportStatusChanged.  Used to gate
	 * OnContentUpdated — we only propagate transform changes to the
	 * app while the user is actively panning (RUNNING) or while inertia
	 * is running (INERTIA).  In ENABLED/READY the transform is at rest;
	 * SyncContentTransform calls we issue ourselves (from
	 * flux_dmanip_viewport_sync_transform) would otherwise loop back
	 * and clobber the very value we just pushed. */
	DIRECTMANIPULATION_STATUS    status;
	/* Last content size written — DManip is picky about zero/negative
	 * sizes, so we cache them and only push when non-zero. */
	int                          content_w;
	int                          content_h;
	/* DIP→pixel scale.  set_rect/set_content_size already take pixels,
	 * so they don't use this.  The soft boundaries are:
	 *   • OnContentUpdated: transforms come in pixels, app wants DIPs
	 *   • sync_transform: app gives DIPs, DManip wants pixels
	 * Default 1.0 (100% DPI). */
	float                        dpi_scale;
	/* User callback for transform updates. */
	FluxDManipContentUpdatedFn   on_updated;
	void                        *on_updated_ctx;
};

static void viewport_set_status(struct FluxDManipViewport *vp, DIRECTMANIPULATION_STATUS s) {
	if (vp) vp->status = s;
}

/* ═══════════════════════════════════════════════════════════════════════
 * OnContentUpdated — reads the current content transform and feeds it
 * back to the app.  Called on UI thread inside flux_dmanip_tick().
 *
 * The transform is a 2D affine [M11 M12 M21 M22 M31 M32] where
 *     x' = M11*x + M21*y + M31
 *     y' = M12*x + M22*y + M32
 * For translation-only (no scale/rotate): M11=M22=1, M12=M21=0,
 * M31=tx, M32=ty.  DManip convention: positive tx/ty = content moved
 * right/down, i.e. user scrolled up/left.  Our FluxScrollData.scroll_x/y
 * uses the opposite sign (scroll_x = amount scrolled right), so we negate.
 * ═══════════════════════════════════════════════════════════════════════ */
static HRESULT STDMETHODCALLTYPE handler_OnContentUpdated(
  IDirectManipulationViewportEventHandler *This, IDirectManipulationViewport *viewport,
  IDirectManipulationContent *content
) {
	( void ) viewport;
	FluxDManipHandler *self = ( FluxDManipHandler * ) This;
	if (!self->owner || !content) return S_OK;

	/* Only trust DManip's transform while the user is actively driving
	 * it.  Any other state (ENABLED/READY/BUILDING/SUSPENDED) may be
	 * echoing back our own SyncContentTransform pushes or stale state. */
	DIRECTMANIPULATION_STATUS st = self->owner->status;
	if (st != DIRECTMANIPULATION_RUNNING && st != DIRECTMANIPULATION_INERTIA) return S_OK;

	float   m [6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	HRESULT hr    = IDirectManipulationContent_GetContentTransform(content, m, 6);
	if (FAILED(hr)) return S_OK;

	/* DManip translation (pixels) → our scroll-offset (DIPs, opposite sign). */
	float inv      = (self->owner->dpi_scale > 0.0f) ? (1.0f / self->owner->dpi_scale) : 1.0f;
	float scroll_x = -m [4] * inv;
	float scroll_y = -m [5] * inv;

	if (self->owner->on_updated) self->owner->on_updated(self->owner->on_updated_ctx, scroll_x, scroll_y);
	return S_OK;
}

/* Static vtable now that all member fn pointers exist. */
static IDirectManipulationViewportEventHandlerVtbl g_handler_vtbl = {
  handler_QueryInterface,
  handler_AddRef,
  handler_Release,
  handler_OnViewportStatusChanged,
  handler_OnViewportUpdated,
  handler_OnContentUpdated,
};

/* ═══════════════════════════════════════════════════════════════════════
 * FluxDManip — manager lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

HRESULT flux_dmanip_create(HWND hwnd, FluxDManip **out) {
	if (!out) return E_POINTER;
	*out = NULL;
	if (!hwnd) return E_INVALIDARG;

	FluxDManip *dm = ( FluxDManip * ) calloc(1, sizeof(*dm));
	if (!dm) return E_OUTOFMEMORY;
	dm->hwnd   = hwnd;

	HRESULT hr = CoCreateInstance(
	  &CLSID_DirectManipulationManager, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectManipulationManager,
	  ( void ** ) &dm->manager
	);
		if (FAILED(hr)) {
			free(dm);
			return hr;
		}

	hr = IDirectManipulationManager_GetUpdateManager(
	  dm->manager, &IID_IDirectManipulationUpdateManager, ( void ** ) &dm->update_mgr
	);
		if (FAILED(hr)) {
			IDirectManipulationManager_Release(dm->manager);
			free(dm);
			return hr;
		}

	/* Activate — associates the manager with this hwnd, unlocks viewport
	 * creation.  Can fail with E_ACCESSDENIED in very old Windows builds
	 * or in sandboxed contexts; the caller can then fall back to the
	 * legacy touch-pan path. */
	hr = IDirectManipulationManager_Activate(dm->manager, hwnd);
		if (FAILED(hr)) {
			IDirectManipulationUpdateManager_Release(dm->update_mgr);
			IDirectManipulationManager_Release(dm->manager);
			free(dm);
			return hr;
		}
	dm->activated = true;

	*out          = dm;
	return S_OK;
}

void flux_dmanip_destroy(FluxDManip *dm) {
	if (!dm) return;
	if (dm->activated && dm->manager) IDirectManipulationManager_Deactivate(dm->manager, dm->hwnd);
	if (dm->update_mgr) IDirectManipulationUpdateManager_Release(dm->update_mgr);
	if (dm->manager) IDirectManipulationManager_Release(dm->manager);
	free(dm);
}

bool flux_dmanip_tick(FluxDManip *dm) {
	if (!dm || !dm->update_mgr) return false;
	/* NULL means "use system frame timing".  The call fires pending
	 * OnContentUpdated / OnViewportStatusChanged handlers. */
	IDirectManipulationUpdateManager_Update(dm->update_mgr, NULL);
	/* We don't (yet) track which viewports are animating — callers can
	 * just always schedule a redraw while a pointer is captured.  Return
	 * false for now; future: walk viewports and check GetStatus. */
	return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 * FluxDManipViewport — per-ScrollView viewport lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

/* Configuration we want for a vertical+horizontal scroller.  Bitwise OR:
 *   INTERACTION        — basic pan gesture recognition
 *   TRANSLATION_X/Y    — allow the content to translate on both axes
 *   TRANSLATION_INERTIA— continue translating after finger lifts
 *   RAILS_X/Y          — lock to one axis when gesture is near-axial
 */
#define FLUX_DM_CONFIG_PAN                                   \
	(DIRECTMANIPULATION_CONFIGURATION_INTERACTION            \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X       \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y       \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA \
	  | DIRECTMANIPULATION_CONFIGURATION_RAILS_X             \
	  | DIRECTMANIPULATION_CONFIGURATION_RAILS_Y)

HRESULT flux_dmanip_viewport_create(FluxDManip *dm, FluxDManipViewport **out) {
	if (!out) return E_POINTER;
	*out = NULL;
	if (!dm || !dm->manager) return E_INVALIDARG;

	FluxDManipViewport *vp = ( FluxDManipViewport * ) calloc(1, sizeof(*vp));
	if (!vp) return E_OUTOFMEMORY;
	vp->dm        = dm;
	vp->dpi_scale = 1.0f;

	HRESULT hr    = IDirectManipulationManager_CreateViewport(
	  dm->manager, NULL, dm->hwnd, &IID_IDirectManipulationViewport, ( void ** ) &vp->viewport
	);
		if (FAILED(hr)) {
			free(vp);
			return hr;
		}

	/* Get the primary content — that's what carries the transform. */
	hr = IDirectManipulationViewport_GetPrimaryContent(
	  vp->viewport, &IID_IDirectManipulationContent, ( void ** ) &vp->primary
	);
		if (FAILED(hr)) {
			IDirectManipulationViewport_Abandon(vp->viewport);
			IDirectManipulationViewport_Release(vp->viewport);
			free(vp);
			return hr;
		}

	/* Add + activate pan configuration. */
	hr = IDirectManipulationViewport_AddConfiguration(vp->viewport, FLUX_DM_CONFIG_PAN);
	if (SUCCEEDED(hr)) hr = IDirectManipulationViewport_ActivateConfiguration(vp->viewport, FLUX_DM_CONFIG_PAN);

	/* MANUALUPDATE — we drive OnContentUpdated timing via flux_dmanip_tick. */
	if (SUCCEEDED(hr))
		hr = IDirectManipulationViewport_SetViewportOptions(
		  vp->viewport, DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE
		);

		/* Attach our event handler. */
		if (SUCCEEDED(hr)) {
			vp->handler = handler_create(vp);
			if (!vp->handler) hr = E_OUTOFMEMORY;
		}
		if (SUCCEEDED(hr)) {
			hr = IDirectManipulationViewport_AddEventHandler(
			  vp->viewport, dm->hwnd, ( IDirectManipulationViewportEventHandler * ) vp->handler, &vp->handler_cookie
			);
		}

		if (FAILED(hr)) {
			flux_dmanip_viewport_destroy(vp);
			return hr;
		}

	dm->live_viewports++;
	*out = vp;
	return S_OK;
}

void flux_dmanip_viewport_destroy(FluxDManipViewport *vp) {
	if (!vp) return;
		if (vp->viewport) {
			if (vp->enabled) IDirectManipulationViewport_Disable(vp->viewport);
			if (vp->handler_cookie) IDirectManipulationViewport_RemoveEventHandler(vp->viewport, vp->handler_cookie);
			IDirectManipulationViewport_Abandon(vp->viewport);
		}
		if (vp->handler) {
			/* Null out the back-pointer before releasing — DManip may still
			 * hold a ref and deliver one final callback. */
			vp->handler->owner = NULL;
			IDirectManipulationViewportEventHandler_Release(( IDirectManipulationViewportEventHandler * ) vp->handler);
		}
	if (vp->primary) IDirectManipulationContent_Release(vp->primary);
	if (vp->viewport) IDirectManipulationViewport_Release(vp->viewport);
	if (vp->dm) vp->dm->live_viewports--;
	free(vp);
}

void flux_dmanip_viewport_set_rect(FluxDManipViewport *vp, int x, int y, int w, int h) {
	if (!vp || !vp->viewport || w <= 0 || h <= 0) return;
	RECT    r  = {x, y, x + w, y + h};
	HRESULT hr = IDirectManipulationViewport_SetViewportRect(vp->viewport, &r);
		/* First time through — enable the viewport once geometry is valid. */
		if (SUCCEEDED(hr) && !vp->enabled) {
			if (SUCCEEDED(IDirectManipulationViewport_Enable(vp->viewport))) vp->enabled = true;
		}
}

void flux_dmanip_viewport_set_content_size(FluxDManipViewport *vp, int w, int h) {
	if (!vp || !vp->primary || w <= 0 || h <= 0) return;
	if (w == vp->content_w && h == vp->content_h) return;
	RECT r = {0, 0, w, h};
		if (SUCCEEDED(IDirectManipulationContent_SetContentRect(vp->primary, &r))) {
			vp->content_w = w;
			vp->content_h = h;
		}
}

HRESULT flux_dmanip_viewport_set_contact(FluxDManipViewport *vp, uint32_t pointer_id) {
	if (!vp || !vp->viewport) return E_INVALIDARG;
	if (!vp->enabled) return E_NOT_VALID_STATE;
	return IDirectManipulationViewport_SetContact(vp->viewport, pointer_id);
}

void flux_dmanip_viewport_set_callback(FluxDManipViewport *vp, FluxDManipContentUpdatedFn cb, void *ctx) {
	if (!vp) return;
	vp->on_updated     = cb;
	vp->on_updated_ctx = ctx;
}

void flux_dmanip_viewport_sync_transform(FluxDManipViewport *vp, float scroll_x, float scroll_y) {
	if (!vp || !vp->primary) return;
	/* Don't fight the user: while DManip owns the gesture, our cached
	 * scroll value is behind its transform by definition.  Refuse to
	 * push \u2014 the OnContentUpdated callback is already writing the
	 * authoritative value back to us. */
	if (vp->status == DIRECTMANIPULATION_RUNNING || vp->status == DIRECTMANIPULATION_INERTIA) return;
	/* Translation-only 2D affine: identity linear part, tx/ty in M31/M32.
	 * DManip sign convention is opposite to ours (see OnContentUpdated).
	 * DManip works in pixels — scale up from DIPs. */
	float s     = (vp->dpi_scale > 0.0f) ? vp->dpi_scale : 1.0f;
	float m [6] = {1.0f, 0.0f, 0.0f, 1.0f, -scroll_x * s, -scroll_y * s};
	( void ) IDirectManipulationContent_SyncContentTransform(vp->primary, m, 6);
}

bool flux_dmanip_viewport_is_manipulating(FluxDManipViewport const *vp) {
	if (!vp) return false;
	return vp->status == DIRECTMANIPULATION_RUNNING || vp->status == DIRECTMANIPULATION_INERTIA;
}

void flux_dmanip_viewport_set_dpi_scale(FluxDManipViewport *vp, float scale) {
	if (!vp) return;
	if (scale > 0.0f) vp->dpi_scale = scale;
}
