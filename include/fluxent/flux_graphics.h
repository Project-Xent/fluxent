/**
 * @file flux_graphics.h
 * @brief DirectX graphics context and composition layer management.
 *
 * FluxGraphics encapsulates Direct2D, DirectWrite, DirectComposition, and
 * swap chain management. It provides the rendering foundation for Fluxent.
 *
 * ## Initialization
 *
 * 1. Create with `flux_graphics_create()`
 * 2. Attach to window with `flux_graphics_attach(hwnd)`
 * 3. Set DPI with `flux_graphics_set_dpi()` (typically from WM_DPICHANGED)
 *
 * ## Frame Rendering
 *
 * ```c
 * flux_graphics_begin_draw(gfx);
 * flux_graphics_clear(gfx, background);
 * // ... issue D2D draw calls ...
 * flux_graphics_end_draw(gfx);
 * flux_graphics_commit(gfx);    // Commit DirectComposition
 * flux_graphics_present(gfx, vsync);
 * ```
 *
 * ## DirectComposition
 *
 * The graphics context creates a DirectComposition visual tree rooted at
 * the swap chain visual. Use `flux_graphics_add_overlay_visual()` to add
 * additional visuals (e.g., for system compositor animations).
 *
 * ## Device Loss
 *
 * Check `flux_graphics_is_device_current()` and call
 * `flux_graphics_handle_device_change()` if the D3D device was lost.
 *
 * ## Thread Safety
 *
 * All functions must be called from the window's thread (typically main).
 */
#ifndef FLUX_GRAPHICS_H
#define FLUX_GRAPHICS_H

#include "flux_types.h"

#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxGraphics        FluxGraphics;

/* Forward declarations for DirectX types */
typedef struct ID2D1DeviceContext  ID2D1DeviceContext;
typedef struct ID2D1Factory3       ID2D1Factory3;
typedef struct IDWriteFactory3     IDWriteFactory3;
typedef struct IDCompositionDevice IDCompositionDevice;
typedef struct IDCompositionVisual IDCompositionVisual;

/* ═══════════════════════════════════════════════════════════════════════
   Lifecycle
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a graphics context.
 *
 * Initializes D3D11, D2D1, DirectWrite, and DirectComposition devices.
 *
 * @return New graphics context, or NULL on failure.
 */
FluxGraphics                      *flux_graphics_create(void);

/**
 * @brief Destroy a graphics context and release all resources.
 * @param gfx Graphics context to destroy (NULL is safe).
 */
void                               flux_graphics_destroy(FluxGraphics *gfx);

/**
 * @brief Attach graphics to a window.
 *
 * Creates swap chain and DirectComposition target for the window.
 * Must be called before any rendering.
 *
 * @param gfx Graphics context.
 * @param hwnd Window handle.
 * @return S_OK on success, or HRESULT error code.
 */
HRESULT                            flux_graphics_attach(FluxGraphics *gfx, HWND hwnd);

/**
 * @brief Resize the swap chain to match window size.
 *
 * Call this in response to WM_SIZE.
 *
 * @param gfx Graphics context.
 * @param width New width in pixels.
 * @param height New height in pixels.
 * @return S_OK on success, or HRESULT error code.
 */
HRESULT                            flux_graphics_resize(FluxGraphics *gfx, int width, int height);

/* ═══════════════════════════════════════════════════════════════════════
   DPI Management
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set the DPI for rendering.
 *
 * The D2D render target will use this DPI for coordinate scaling.
 * Call in response to WM_DPICHANGED.
 *
 * @param gfx Graphics context.
 * @param dpi DPI information.
 */
void                               flux_graphics_set_dpi(FluxGraphics *gfx, FluxDpiInfo dpi);

/**
 * @brief Get the current DPI.
 * @param gfx Graphics context.
 * @return Current DPI information.
 */
FluxDpiInfo                        flux_graphics_get_dpi(FluxGraphics const *gfx);

/* ═══════════════════════════════════════════════════════════════════════
   Frame Rendering
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Begin a render frame.
 *
 * Acquires the back buffer and begins D2D drawing.
 * Must be paired with flux_graphics_end_draw().
 *
 * @param gfx Graphics context.
 */
void                               flux_graphics_begin_draw(FluxGraphics *gfx);

/**
 * @brief End the render frame.
 *
 * Completes D2D drawing. Call flux_graphics_commit() and
 * flux_graphics_present() after this.
 *
 * @param gfx Graphics context.
 */
void                               flux_graphics_end_draw(FluxGraphics *gfx);

/**
 * @brief Present the rendered frame.
 *
 * Swaps the back buffer to display.
 *
 * @param gfx Graphics context.
 * @param vsync true to wait for vertical blank, false for immediate.
 */
void                               flux_graphics_present(FluxGraphics *gfx, bool vsync);

/**
 * @brief Clear the render target to a solid color.
 * @param gfx Graphics context.
 * @param color Fill color.
 */
void                               flux_graphics_clear(FluxGraphics *gfx, FluxColor color);

/**
 * @brief Commit the DirectComposition visual tree.
 *
 * Call after flux_graphics_end_draw() to apply visual tree changes.
 *
 * @param gfx Graphics context.
 */
void                               flux_graphics_commit(FluxGraphics *gfx);

/* ═══════════════════════════════════════════════════════════════════════
   Size Queries
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the render target size in DIPs.
 * @param gfx Graphics context.
 * @return Target size.
 */
FluxSize                           flux_graphics_get_target_size(FluxGraphics const *gfx);

/**
 * @brief Get the client area size in physical pixels.
 * @param gfx Graphics context.
 * @return Client size in pixels.
 */
FluxSize                           flux_graphics_get_client_pixel_size(FluxGraphics const *gfx);

/* ═══════════════════════════════════════════════════════════════════════
   DirectX Resource Access
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the D2D device context.
 *
 * Use for issuing D2D draw calls. Valid only between begin/end_draw.
 *
 * @param gfx Graphics context.
 * @return D2D device context (caller does NOT own; do not Release).
 */
ID2D1DeviceContext                *flux_graphics_get_d2d_context(FluxGraphics *gfx);

/**
 * @brief Get the D2D factory.
 * @param gfx Graphics context.
 * @return D2D factory (caller does NOT own; do not Release).
 */
ID2D1Factory3                     *flux_graphics_get_d2d_factory(FluxGraphics *gfx);

/**
 * @brief Get the DirectWrite factory.
 * @param gfx Graphics context.
 * @return DWrite factory (caller does NOT own; do not Release).
 */
IDWriteFactory3                   *flux_graphics_get_dwrite_factory(FluxGraphics *gfx);

/**
 * @brief Get the DirectComposition device.
 * @param gfx Graphics context.
 * @return DComp device (caller does NOT own; do not Release).
 */
IDCompositionDevice               *flux_graphics_get_dcomp_device(FluxGraphics *gfx);

/**
 * @brief Get the root DirectComposition visual.
 * @param gfx Graphics context.
 * @return Root visual (caller does NOT own; do not Release).
 */
IDCompositionVisual               *flux_graphics_get_root_visual(FluxGraphics *gfx);

/* ═══════════════════════════════════════════════════════════════════════
   Visual Management
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add an overlay visual to the composition tree.
 *
 * Overlay visuals are rendered on top of the main swap chain content.
 * Used for system compositor animations (e.g., Connected Animations).
 *
 * @param gfx Graphics context.
 * @param visual Visual to add (takes ownership; will be Released on remove).
 */
void                               flux_graphics_add_overlay_visual(FluxGraphics *gfx, IDCompositionVisual *visual);

/**
 * @brief Request a redraw on the next frame.
 *
 * Schedules a WM_PAINT message for the window.
 *
 * @param gfx Graphics context.
 */
void                               flux_graphics_request_redraw(FluxGraphics *gfx);

/**
 * @brief Redraw callback type.
 */
typedef void                       (*FluxRedrawCallback)(void *ctx);

/**
 * @brief Set the redraw callback.
 *
 * Called when a redraw is needed (e.g., animation tick, device change).
 *
 * @param gfx Graphics context.
 * @param cb Callback function.
 * @param ctx User context passed to callback.
 */
void    flux_graphics_set_redraw_callback(FluxGraphics *gfx, FluxRedrawCallback cb, void *ctx);

/* ═══════════════════════════════════════════════════════════════════════
   Device Management
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if the D3D device is still valid.
 *
 * Returns false if device was lost and needs recreation.
 *
 * @param gfx Graphics context.
 * @return true if device is valid.
 */
bool    flux_graphics_is_device_current(FluxGraphics const *gfx);

/**
 * @brief Handle device loss and recreate resources.
 *
 * Call this if is_device_current() returns false.
 *
 * @param gfx Graphics context.
 * @return S_OK on success, or HRESULT error code.
 */
HRESULT flux_graphics_handle_device_change(FluxGraphics *gfx);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_GRAPHICS_H */
