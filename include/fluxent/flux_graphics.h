#ifndef FLUX_GRAPHICS_H
#define FLUX_GRAPHICS_H

#include "flux_types.h"

#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxGraphics FluxGraphics;

typedef struct ID2D1DeviceContext ID2D1DeviceContext;
typedef struct ID2D1Factory3 ID2D1Factory3;
typedef struct IDWriteFactory3 IDWriteFactory3;
typedef struct IDCompositionDevice IDCompositionDevice;
typedef struct IDCompositionVisual IDCompositionVisual;

FluxGraphics *flux_graphics_create(void);
void          flux_graphics_destroy(FluxGraphics *gfx);

HRESULT       flux_graphics_attach(FluxGraphics *gfx, HWND hwnd);
HRESULT       flux_graphics_resize(FluxGraphics *gfx, int width, int height);
void          flux_graphics_set_dpi(FluxGraphics *gfx, FluxDpiInfo dpi);
FluxDpiInfo   flux_graphics_get_dpi(const FluxGraphics *gfx);

void          flux_graphics_begin_draw(FluxGraphics *gfx);
void          flux_graphics_end_draw(FluxGraphics *gfx);
void          flux_graphics_present(FluxGraphics *gfx, bool vsync);
void          flux_graphics_clear(FluxGraphics *gfx, FluxColor color);
void          flux_graphics_commit(FluxGraphics *gfx);

FluxSize      flux_graphics_get_target_size(const FluxGraphics *gfx);
FluxSize      flux_graphics_get_client_pixel_size(const FluxGraphics *gfx);

ID2D1DeviceContext   *flux_graphics_get_d2d_context(FluxGraphics *gfx);
ID2D1Factory3        *flux_graphics_get_d2d_factory(FluxGraphics *gfx);
IDWriteFactory3      *flux_graphics_get_dwrite_factory(FluxGraphics *gfx);
IDCompositionDevice  *flux_graphics_get_dcomp_device(FluxGraphics *gfx);
IDCompositionVisual  *flux_graphics_get_root_visual(FluxGraphics *gfx);

void          flux_graphics_add_overlay_visual(FluxGraphics *gfx, IDCompositionVisual *visual);
void          flux_graphics_request_redraw(FluxGraphics *gfx);

typedef void (*FluxRedrawCallback)(void *ctx);
void          flux_graphics_set_redraw_callback(FluxGraphics *gfx, FluxRedrawCallback cb, void *ctx);

bool          flux_graphics_is_device_current(const FluxGraphics *gfx);
HRESULT       flux_graphics_handle_device_change(FluxGraphics *gfx);

#ifdef __cplusplus
}
#endif

#endif