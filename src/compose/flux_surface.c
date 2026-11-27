#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_surface.h"
#include "compose/flux_interop.h"

#include <cwinrt/cast.h>
#include <cd2d.h>

#include <stdbool.h>
#include <stdlib.h>

struct FluxSurface {
	WUC_CompositionDrawingSurface         *surface;
	FluxICompositionDrawingSurfaceInterop *interop;
	WUC_CompositionSurfaceBrush           *brush;
	ID2D1DeviceContext                    *active_dc;
	bool                                   has_drawn;
};

static int   clamp_dim(int v) { return v < 1 ? 1 : v; }

FluxSurface *flux_surface_create(FluxCompositor *c, WUC_CompositionGraphicsDevice *device, int w, int h) {
	if (!c || !device) return NULL;
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxSurface *s = ( FluxSurface * ) calloc(1, sizeof(*s));
	if (!s) return NULL;

	WF_Size size = {( float ) clamp_dim(w), ( float ) clamp_dim(h)};
	HRESULT hr   = wuc_composition_graphics_device_create_drawing_surface(
	  device, size, WGRDI_DirectXPixelFormat_B8G8R8A8UIntNormalized, WGRDI_DirectXAlphaMode_Premultiplied, &s->surface
	);
	if (FAILED(hr)) goto fail;

	hr = cwinrt_query(s->surface, &FluxIID_ICompositionDrawingSurfaceInterop, ( void ** ) &s->interop);
	if (FAILED(hr)) goto fail;

	WUC_ICompositionSurface *as_surface = NULL;
	hr = cwinrt_query(s->surface, &CWINRT_IID_WUC_ICompositionSurface, ( void ** ) &as_surface);
	if (FAILED(hr)) goto fail;

	hr = wuc_comp_create_surface_brush_p_p(comp, as_surface, &s->brush);
	(( IUnknown * ) as_surface)->lpVtbl->Release(( IUnknown * ) as_surface);
	if (FAILED(hr)) goto fail;

	return s;

fail:
	flux_surface_destroy(s);
	return NULL;
}

void flux_surface_destroy(FluxSurface *s) {
	if (!s) return;
	if (s->brush) (( IUnknown * ) s->brush)->lpVtbl->Release(( IUnknown * ) s->brush);
	if (s->interop) s->interop->lpVtbl->Release(s->interop);
	if (s->surface) (( IUnknown * ) s->surface)->lpVtbl->Release(( IUnknown * ) s->surface);
	free(s);
}

HRESULT flux_surface_resize(FluxSurface *s, int w, int h) {
	if (!s || !s->surface) return E_INVALIDARG;
	s->has_drawn      = false;
	WG_SizeInt32 size = {clamp_dim(w), clamp_dim(h)};
	return wuc_composition_drawing_surface_resize(s->surface, size);
}

static RECT surface_clamp_rect(WG_SizeInt32 size, RECT const *dirty) {
	RECT r = *dirty;
	if (r.left < 0) r.left = 0;
	if (r.top < 0) r.top = 0;
	if (r.right > size.Width) r.right = size.Width;
	if (r.bottom > size.Height) r.bottom = size.Height;
	if (r.right <= r.left || r.bottom <= r.top) r = (RECT) {0, 0, size.Width, size.Height};
	return r;
}

ID2D1DeviceContext *flux_surface_begin(FluxSurface *s, RECT const *dirty, POINT *out_offset, RECT *out_used) {
	if (!s || !s->interop) return NULL;

	WG_SizeInt32 size = {0, 0};
	if (FAILED(wuc_composition_drawing_surface_get__size_int32(s->surface, &size))) return NULL;

	RECT    whole  = {0, 0, size.Width, size.Height};
	RECT    rect   = (dirty && s->has_drawn) ? surface_clamp_rect(size, dirty) : whole;
	POINT   offset = {0, 0};
	void   *dc     = NULL;
	HRESULT hr     = s->interop->lpVtbl->BeginDraw(s->interop, &rect, &IID_ID2D1DeviceContext, &dc, &offset);
	if (FAILED(hr)) return NULL;

	s->has_drawn = true;
	s->active_dc = ( ID2D1DeviceContext * ) dc;
	if (out_offset) *out_offset = offset;
	if (out_used) *out_used = rect;
	return s->active_dc;
}

void flux_surface_end(FluxSurface *s) {
	if (!s || !s->interop) return;
	if (s->active_dc) {
		(( IUnknown * ) s->active_dc)->lpVtbl->Release(( IUnknown * ) s->active_dc);
		s->active_dc = NULL;
	}
	( void ) s->interop->lpVtbl->EndDraw(s->interop);
}

WUC_CompositionSurfaceBrush *flux_surface_brush(FluxSurface *s) { return s ? s->brush : NULL; }
