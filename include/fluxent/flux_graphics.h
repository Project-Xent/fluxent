/**
 * @file flux_graphics.h
 * @brief DirectX graphics context and composition layer management.
 *
 * FluxGfx encapsulates Direct2D, DirectWrite, DirectComposition, and
 * swap chain management. It provides the rendering foundation for Fluxent.
 *
 * ## Initialization
 *
 * 1. Create with `flux_gfx_create()`
 * 2. Attach to window with `flux_gfx_attach(hwnd)`
 * 3. Set DPI with `flux_gfx_setdpi()` (typically from WM_DPICHANGED)
 *
 * ## Frame Rendering
 *
 * ```c
 * flux_gfx_begin(gfx);
 * flux_gfx_clear(gfx, background);
 * // ... issue D2D draw calls ...
 * flux_gfx_end(gfx);
 * flux_gfx_commit(gfx);    // Commit DirectComposition
 * flux_gfx_present(gfx, vsync);
 * ```
 *
 * ## DirectComposition
 *
 * The graphics context creates a DirectComposition visual tree rooted at
 * the swap chain visual.
 *
 * ## Device Loss
 *
 * Check `flux_gfx_current()` and call
 * `flux_gfx_devicechange()` if the D3D device was lost.
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


typedef struct FluxGfx             FluxGfx;

typedef struct ID2D1DeviceContext  ID2D1DeviceContext;
typedef struct ID2D1Factory3       ID2D1Factory3;
typedef struct IDWriteFactory3     IDWriteFactory3;
typedef struct IDCompositionDevice IDCompositionDevice;
typedef struct IDCompositionVisual IDCompositionVisual;

/**
 * @brief Create a graphics context.
 *
 * Initializes D3D11, D2D1, DirectWrite, and DirectComposition devices.
 *
 * @return New graphics context, or NULL on failure.
 */
XENT_NODISCARD FluxGfx            *flux_gfx_create(void);

/**
 * @brief Destroy a graphics context and release all resources.
 * @param gfx Graphics context to destroy (NULL is safe).
 */
void                               flux_gfx_destroy(FluxGfx *gfx);

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
HRESULT                            flux_gfx_attach(FluxGfx *gfx, HWND hwnd);

/**
 * @brief Attach graphics directly to an HWND swap chain.
 *
 * Creates a swap chain bound directly to the HWND instead of a
 * DirectComposition visual tree. Use only for windows that are known to
 * support direct HWND swap-chain presentation; popup windows in Fluxent use
 * flux_gfx_attach() because they rely on the DirectComposition path.
 *
 * @param gfx Graphics context.
 * @param hwnd Window handle.
 * @return S_OK on success, or HRESULT error code.
 */
HRESULT                            flux_gfx_attachbackdrop(FluxGfx *gfx, HWND hwnd);

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
HRESULT                            flux_gfx_resize(FluxGfx *gfx, int width, int height);

/**
 * @brief Set the DPI for rendering.
 *
 * The D2D render target will use this DPI for coordinate scaling.
 * Call in response to WM_DPICHANGED.
 *
 * @param gfx Graphics context.
 * @param dpi DPI information.
 */
void                               flux_gfx_setdpi(FluxGfx *gfx, FluxDpi dpi);

/**
 * @brief Get the current DPI.
 * @param gfx Graphics context.
 * @return Current DPI information.
 */
FluxDpi                            flux_gfx_dpi(FluxGfx const *gfx);

/**
 * @brief Begin a render frame.
 *
 * Acquires the back buffer and begins D2D drawing.
 * Must be paired with flux_gfx_end().
 *
 * @param gfx Graphics context.
 */
void                               flux_gfx_begin(FluxGfx *gfx);

/**
 * @brief End the render frame.
 *
 * Completes D2D drawing. Call flux_gfx_commit() and
 * flux_gfx_present() after this.
 *
 * @param gfx Graphics context.
 */
void                               flux_gfx_end(FluxGfx *gfx);

/**
 * @brief Present the rendered frame.
 *
 * Swaps the back buffer to display.
 *
 * @param gfx Graphics context.
 * @param vsync true to wait for vertical blank, false for immediate.
 */
void                               flux_gfx_present(FluxGfx *gfx, bool vsync);

/**
 * @brief Clear the render target to a solid color.
 * @param gfx Graphics context.
 * @param color Fill color.
 */
void                               flux_gfx_clear(FluxGfx *gfx, FluxColor color);

/**
 * @brief Commit the DirectComposition visual tree.
 *
 * Call after flux_gfx_end() to apply visual tree changes.
 *
 * @param gfx Graphics context.
 */
void                               flux_gfx_commit(FluxGfx *gfx);

/**
 * @brief Get the render target size in DIPs.
 * @param gfx Graphics context.
 * @return Target size.
 */
FluxSize                           flux_gfx_size(FluxGfx const *gfx);

/**
 * @brief Get the client area size in physical pixels.
 * @param gfx Graphics context.
 * @return Client size in pixels.
 */
FluxSize                           flux_gfx_pixels(FluxGfx const *gfx);

/**
 * @brief Get the D2D device context.
 *
 * Use for issuing D2D draw calls. Valid only between begin/end_draw.
 *
 * @param gfx Graphics context.
 * @return D2D device context (caller does NOT own; do not Release).
 */
ID2D1DeviceContext                *flux_gfx_d2d(FluxGfx *gfx);

/**
 * @brief Get the D2D factory.
 * @param gfx Graphics context.
 * @return D2D factory (caller does NOT own; do not Release).
 */
ID2D1Factory3                     *flux_gfx_d2dfactory(FluxGfx *gfx);

/**
 * @brief Get the DirectWrite factory.
 * @param gfx Graphics context.
 * @return DWrite factory (caller does NOT own; do not Release).
 */
IDWriteFactory3                   *flux_gfx_dwrite(FluxGfx *gfx);


/**
 * @brief Request a redraw on the next frame.
 *
 * Schedules a WM_PAINT message for the window.
 *
 * @param gfx Graphics context.
 */
void                               flux_gfx_redraw(FluxGfx *gfx);

/**
 * @brief Redraw callback type.
 */
typedef void                       (*FluxRedrawFn)(void *ctx);

/**
 * @brief Set the redraw callback.
 *
 * Called when a redraw is needed (e.g., animation tick, device change).
 *
 * @param gfx Graphics context.
 * @param cb Callback function.
 * @param ctx User context passed to callback.
 */
void                               flux_gfx_onredraw(FluxGfx *gfx, FluxRedrawFn cb, void *ctx);

/**
 * @brief Check if the D3D device is still valid.
 *
 * Returns false if device was lost and needs recreation.
 *
 * @param gfx Graphics context.
 * @return true if device is valid.
 */
bool                               flux_gfx_current(FluxGfx const *gfx);

/**
 * @brief Handle device loss and recreate resources.
 *
 * Call this if is_device_current() returns false.
 *
 * @param gfx Graphics context.
 * @return S_OK on success, or HRESULT error code.
 */
HRESULT                            flux_gfx_devicechange(FluxGfx *gfx);


#endif
