#ifndef FLUX_COMPOSE_H
#define FLUX_COMPOSE_H

#include <windows.h>

#include <cwinrt/Windows.UI.Composition.h>

typedef struct FluxCompositor   FluxCompositor;
typedef struct FluxWindowTarget FluxWindowTarget;

FluxCompositor                 *flux_compositor_acquire(void);

void                            flux_compositor_release(FluxCompositor *c);

WUC_Comp                       *flux_compositor_comp(FluxCompositor *c);

WUC_Visual                     *flux_compose_as_visual(void *composition_object);

WUC_CompositionGraphicsDevice  *flux_compositor_dev_create(FluxCompositor *c, IUnknown *d2d_device);

void                            flux_compositor_dev_destroy(WUC_CompositionGraphicsDevice *device);

FluxWindowTarget               *flux_window_target_create(FluxCompositor *c, HWND hwnd);

void                            flux_window_target_destroy(FluxWindowTarget *t);

HRESULT                         flux_wintarget_setswapchain(FluxWindowTarget *t, IUnknown *swapchain);

HRESULT                         flux_wintarget_presentchild(FluxWindowTarget *t, IUnknown *swapchain);

void                            flux_wintarget_setbackdrop(FluxWindowTarget *t, WUC_Brush *brush);

void                            flux_wintarget_setsize(FluxWindowTarget *t, float w, float h);
void                            flux_wintarget_setopacity(FluxWindowTarget *t, float opacity);
void        flux_wintarget_setplate(FluxWindowTarget *t, float x, float y, float w, float h, float radius);

WUC_Visual *flux_window_target_root(FluxWindowTarget *t);

#endif
