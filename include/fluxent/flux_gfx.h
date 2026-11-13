#ifndef FLUX_GFX_H
#define FLUX_GFX_H

#include "flux_types.h"

#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>
#endif

typedef struct FluxGfx             FluxGfx;

typedef struct ID2D1DeviceContext  ID2D1DeviceContext;
typedef struct ID2D1Factory3       ID2D1Factory3;
typedef struct IDWriteFactory3     IDWriteFactory3;
typedef struct IDCompositionDevice IDCompositionDevice;
typedef struct IDCompositionVisual IDCompositionVisual;

XENT_NODISCARD FluxGfx            *flux_gfx_create(HRESULT *out_hr);

void                               flux_gfx_destroy(FluxGfx *gfx);

HRESULT                            flux_gfx_attach(FluxGfx *gfx, HWND hwnd);

HRESULT                            flux_gfx_attachbackdrop(FluxGfx *gfx, HWND hwnd);

HRESULT                            flux_gfx_resize(FluxGfx *gfx, int width, int height);

void                               flux_gfx_setdpi(FluxGfx *gfx, FluxDpi dpi);

FluxDpi                            flux_gfx_dpi(FluxGfx const *gfx);

void                               flux_gfx_begin(FluxGfx *gfx);

void                               flux_gfx_end(FluxGfx *gfx);

void                               flux_gfx_present(FluxGfx *gfx, bool vsync);

void                               flux_gfx_clear(FluxGfx *gfx, FluxColor color);

void                               flux_gfx_commit(FluxGfx *gfx);

FluxSize                           flux_gfx_size(FluxGfx const *gfx);

FluxSize                           flux_gfx_pixels(FluxGfx const *gfx);

ID2D1DeviceContext                *flux_gfx_d2d(FluxGfx *gfx);

ID2D1Factory3                     *flux_gfx_d2dfactory(FluxGfx *gfx);

IDWriteFactory3                   *flux_gfx_dwrite(FluxGfx *gfx);

void                               flux_gfx_redraw(FluxGfx *gfx);

typedef void                       (*FluxRedrawFn)(void *ctx);

void                               flux_gfx_onredraw(FluxGfx *gfx, FluxRedrawFn cb, void *ctx);

bool                               flux_gfx_current(FluxGfx const *gfx);

HRESULT                            flux_gfx_devicechange(FluxGfx *gfx);

bool                               flux_gfx_software(FluxGfx const *gfx);

#endif
