#ifndef FLUX_DMANIP_H
#define FLUX_DMANIP_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct FluxDManip         FluxDManip;
typedef struct FluxDManipViewport FluxDManipViewport;

typedef void                      (*FluxDManipContentUpdatedFn)(void *ctx, float scroll_x, float scroll_y);

HRESULT                           flux_dmanip_create(HWND hwnd, FluxDManip **out);

void                              flux_dmanip_destroy(FluxDManip *dm);

bool                              flux_dmanip_tick(FluxDManip *dm);

HRESULT                           flux_dmanip_viewport_create(FluxDManip *dm, FluxDManipViewport **out);

void                              flux_dmanip_viewport_destroy(FluxDManipViewport *vp);

void                              flux_dmanip_vp_setrect(FluxDManipViewport *vp, int x, int y, int w, int h);

void                              flux_dmanip_vp_setsize(FluxDManipViewport *vp, int w, int h);

void                              flux_dmanip_vp_setdpi(FluxDManipViewport *vp, float scale);

void                              flux_dmanip_vp_setsnapy(FluxDManipViewport *vp, float interval);

HRESULT                           flux_dmanip_vp_setcontact(FluxDManipViewport *vp, uint32_t pointer_id);

void flux_dmanip_vp_onchange(FluxDManipViewport *vp, FluxDManipContentUpdatedFn cb, void *ctx);

void flux_dmanip_vp_sync(FluxDManipViewport *vp, float scroll_x, float scroll_y);

#endif
