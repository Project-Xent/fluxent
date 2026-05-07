#include "flux_dmanip.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <directmanipulation.h>
#include <objbase.h>
#include <assert.h>
#include <stdlib.h>

typedef struct FluxDManipHandler {
	IDirectManipulationViewportEventHandlerVtbl *lpVtbl;
	LONG                                         ref_count;
	struct FluxDManipViewport                   *owner;
} FluxDManipHandler;

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

struct FluxDManipViewport;
static void                      viewport_set_status(struct FluxDManipViewport *vp, DIRECTMANIPULATION_STATUS s);

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

struct FluxDManip {
	HWND                              hwnd;
	IDirectManipulationManager       *manager;
	IDirectManipulationUpdateManager *update_mgr;
	DWORD                             ui_thread_id;
	bool                              activated;
	int                               live_viewports;
};

struct FluxDManipViewport {
	FluxDManip                  *dm;
	IDirectManipulationViewport *viewport;
	IDirectManipulationContent  *primary;
	FluxDManipHandler           *handler;
	DWORD                        handler_cookie;
	bool                         enabled;
	bool                         counted;
	DIRECTMANIPULATION_STATUS    status;
	int                          content_w;
	int                          content_h;
	float                        dpi_scale;
	FluxDManipContentUpdatedFn   on_updated;
	void                        *on_updated_ctx;
};

static void viewport_set_status(struct FluxDManipViewport *vp, DIRECTMANIPULATION_STATUS s) {
	if (vp) vp->status = s;
}

static bool dmanip_is_ui_thread(FluxDManipViewport const *vp) {
	return vp && vp->dm && GetCurrentThreadId() == vp->dm->ui_thread_id;
}

static HRESULT STDMETHODCALLTYPE handler_OnContentUpdated(
  IDirectManipulationViewportEventHandler *This, IDirectManipulationViewport *viewport,
  IDirectManipulationContent *content
) {
	( void ) viewport;
	FluxDManipHandler *self = ( FluxDManipHandler * ) This;
	if (!self->owner || !content) return S_OK;
	assert(dmanip_is_ui_thread(self->owner));
	if (!dmanip_is_ui_thread(self->owner)) return S_OK;

	DIRECTMANIPULATION_STATUS st = self->owner->status;
	if (st != DIRECTMANIPULATION_RUNNING && st != DIRECTMANIPULATION_INERTIA) return S_OK;

	float   m [6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	HRESULT hr    = IDirectManipulationContent_GetContentTransform(content, m, 6);
	if (FAILED(hr)) return S_OK;

	float inv      = (self->owner->dpi_scale > 0.0f) ? (1.0f / self->owner->dpi_scale) : 1.0f;
	float scroll_x = -m [4] * inv;
	float scroll_y = -m [5] * inv;

	if (self->owner->on_updated) self->owner->on_updated(self->owner->on_updated_ctx, scroll_x, scroll_y);
	return S_OK;
}

static IDirectManipulationViewportEventHandlerVtbl g_handler_vtbl = {
  handler_QueryInterface,
  handler_AddRef,
  handler_Release,
  handler_OnViewportStatusChanged,
  handler_OnViewportUpdated,
  handler_OnContentUpdated,
};

HRESULT flux_dmanip_create(HWND hwnd, FluxDManip **out) {
	if (!out) return E_POINTER;
	*out = NULL;
	if (!hwnd) return E_INVALIDARG;

	FluxDManip *dm = ( FluxDManip * ) calloc(1, sizeof(*dm));
	if (!dm) return E_OUTOFMEMORY;
	dm->hwnd         = hwnd;
	dm->ui_thread_id = GetCurrentThreadId();

	HRESULT hr       = CoCreateInstance(
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
	IDirectManipulationUpdateManager_Update(dm->update_mgr, NULL);
	return false;
}

#define FLUX_DM_CONFIG_PAN                                   \
	(DIRECTMANIPULATION_CONFIGURATION_INTERACTION            \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X       \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y       \
	  | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA \
	  | DIRECTMANIPULATION_CONFIGURATION_RAILS_X             \
	  | DIRECTMANIPULATION_CONFIGURATION_RAILS_Y)

static FluxDManipViewport *dmanip_viewport_alloc(FluxDManip *dm) {
	FluxDManipViewport *vp = ( FluxDManipViewport * ) calloc(1, sizeof(*vp));
	if (!vp) return NULL;
	vp->dm        = dm;
	vp->dpi_scale = 1.0f;
	return vp;
}

static HRESULT dmanip_viewport_create_native(FluxDManipViewport *vp) {
	return IDirectManipulationManager_CreateViewport(
	  vp->dm->manager, NULL, vp->dm->hwnd, &IID_IDirectManipulationViewport, ( void ** ) &vp->viewport
	);
}

static HRESULT dmanip_viewport_get_primary(FluxDManipViewport *vp) {
	return IDirectManipulationViewport_GetPrimaryContent(
	  vp->viewport, &IID_IDirectManipulationContent, ( void ** ) &vp->primary
	);
}

static HRESULT dmanip_viewport_configure(FluxDManipViewport *vp) {
	HRESULT hr = IDirectManipulationViewport_AddConfiguration(vp->viewport, FLUX_DM_CONFIG_PAN);
	if (SUCCEEDED(hr)) hr = IDirectManipulationViewport_ActivateConfiguration(vp->viewport, FLUX_DM_CONFIG_PAN);
	if (SUCCEEDED(hr))
		hr = IDirectManipulationViewport_SetViewportOptions(
		  vp->viewport, DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE
		);
	return hr;
}

static HRESULT dmanip_viewport_add_handler(FluxDManipViewport *vp) {
	vp->handler = handler_create(vp);
	if (!vp->handler) return E_OUTOFMEMORY;

	return IDirectManipulationViewport_AddEventHandler(
	  vp->viewport, vp->dm->hwnd, ( IDirectManipulationViewportEventHandler * ) vp->handler, &vp->handler_cookie
	);
}

static HRESULT dmanip_viewport_init(FluxDManipViewport *vp) {
	HRESULT hr = dmanip_viewport_create_native(vp);
	if (SUCCEEDED(hr)) hr = dmanip_viewport_get_primary(vp);
	if (SUCCEEDED(hr)) hr = dmanip_viewport_configure(vp);
	if (SUCCEEDED(hr)) hr = dmanip_viewport_add_handler(vp);
	return hr;
}

HRESULT flux_dmanip_viewport_create(FluxDManip *dm, FluxDManipViewport **out) {
	if (!out) return E_POINTER;
	*out = NULL;
	if (!dm || !dm->manager) return E_INVALIDARG;

	FluxDManipViewport *vp = dmanip_viewport_alloc(dm);
	if (!vp) return E_OUTOFMEMORY;

	HRESULT hr = dmanip_viewport_init(vp);
	if (FAILED(hr)) {
		flux_dmanip_viewport_destroy(vp);
		return hr;
	}

	dm->live_viewports++;
	vp->counted = true;
	*out        = vp;
	return S_OK;
}

static void dmanip_viewport_shutdown_native(FluxDManipViewport *vp) {
	if (!vp->viewport) return;
	if (vp->enabled) IDirectManipulationViewport_Disable(vp->viewport);
	if (vp->handler_cookie) IDirectManipulationViewport_RemoveEventHandler(vp->viewport, vp->handler_cookie);
	IDirectManipulationViewport_Abandon(vp->viewport);
}

static void dmanip_viewport_release_handler(FluxDManipViewport *vp) {
	if (!vp->handler) return;
	vp->handler->owner = NULL;
	IDirectManipulationViewportEventHandler_Release(( IDirectManipulationViewportEventHandler * ) vp->handler);
}

static void dmanip_viewport_release_native(FluxDManipViewport *vp) {
	if (vp->primary) IDirectManipulationContent_Release(vp->primary);
	if (vp->viewport) IDirectManipulationViewport_Release(vp->viewport);
}

void flux_dmanip_viewport_destroy(FluxDManipViewport *vp) {
	if (!vp) return;
	dmanip_viewport_shutdown_native(vp);
	dmanip_viewport_release_handler(vp);
	dmanip_viewport_release_native(vp);
	if (vp->dm && vp->counted) vp->dm->live_viewports--;
	free(vp);
}

void flux_dmanip_viewport_set_rect(FluxDManipViewport *vp, int x, int y, int w, int h) {
	if (!vp || !vp->viewport || w <= 0 || h <= 0) return;
	RECT    r  = {x, y, x + w, y + h};
	HRESULT hr = IDirectManipulationViewport_SetViewportRect(vp->viewport, &r);
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
	if (vp->status == DIRECTMANIPULATION_RUNNING || vp->status == DIRECTMANIPULATION_INERTIA) return;
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
