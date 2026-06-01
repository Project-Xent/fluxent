/**
 * @file flux_surface.h
 * @brief CompositionDrawingSurface bridge: Direct2D content into the WUC tree.
 *
 * A `FluxSurface` is a GPU surface the existing Direct2D/DirectWrite renderer
 * draws into, exposed to the composition tree as a `CompositionSurfaceBrush`.
 * This is how text and other raster content reach a retained visual: WUC has
 * no text primitive, so glyphs are rasterized by Direct2D (reusing its internal
 * glyph atlas) into a per-element surface, then composited as a brush — the
 * same strategy WinUI 3 itself uses.
 *
 * ## Drawing
 *
 * ```c
 * POINT offset;
 * ID2D1DeviceContext *dc = flux_surface_begin(s, &offset);
 * // Translate all drawing by (offset.x, offset.y): the surface may be atlased,
 * // so the usable region does not start at the origin.
 * flux_surface_end(s);
 * ```
 */
#ifndef FLUX_COMPOSE_SURFACE_H
#define FLUX_COMPOSE_SURFACE_H

#include "compose/flux_compose.h"

typedef struct ID2D1DeviceContext ID2D1DeviceContext;

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxSurface FluxSurface;

/**
 * @brief Create a drawing surface of the given pixel size.
 *
 * @param c      Shared compositor (provides the SurfaceBrush).
 * @param device Graphics device from flux_compositor_create_graphics_device().
 * @param w      Width in physical pixels (clamped to >= 1).
 * @param h      Height in physical pixels (clamped to >= 1).
 * @return New surface, or NULL on failure.
 */
FluxSurface               *flux_surface_create(FluxCompositor *c, WUC_CompositionGraphicsDevice *device, int w, int h);

/**
 * @brief Destroy a surface and its brush. NULL is safe.
 */
void                       flux_surface_destroy(FluxSurface *s);

/**
 * @brief Resize the surface's pixel extent. Content must be redrawn after.
 */
HRESULT                    flux_surface_resize(FluxSurface *s, int w, int h);

/**
 * @brief Begin drawing; returns a Direct2D context positioned for this surface.
 *
 * @param s          Surface.
 * @param out_offset Receives the top-left of the usable region; all drawing
 *                   must be translated by this offset.
 * @return Direct2D device context (released by flux_surface_end), or NULL.
 */
ID2D1DeviceContext        *flux_surface_begin(FluxSurface *s, POINT *out_offset);

/**
 * @brief End drawing started by flux_surface_begin().
 */
void                       flux_surface_end(FluxSurface *s);

/**
 * @brief The SurfaceBrush wrapping this surface (not owned). Assign to a visual.
 */
WUC_CompositionSurfaceBrush *flux_surface_brush(FluxSurface *s);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_SURFACE_H */
