/**
 * @file flux_dmanip.h
 * @brief Pure-C wrapper around DirectManipulation (DManip).
 *
 * DManip is the compositor-aware gesture/inertia engine underpinning WinUI's ScrollViewer:
 * 1:1 finger tracking, physical inertia, rubber-band over-pan, axis rails, and multi-touch
 * handoff via pointer contact IDs.
 *
 * Runs in host-update mode: OnContentUpdated() callbacks write transforms back to
 * FluxScrollData.scroll_x/y; the compositor animates smoothly without custom inertia math.
 */

#ifndef FLUX_DMANIP_H
#define FLUX_DMANIP_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Per-window DirectManipulation manager handle. */
typedef struct FluxDManip         FluxDManip;
/** @brief Per-scroll-view DirectManipulation viewport handle. */
typedef struct FluxDManipViewport FluxDManipViewport;

/** @brief Callback fired on each DManip transform update (pan, inertia, rubber-band).
 *  Always called on the UI thread inside flux_dmanip_tick(). */
typedef void                      (*FluxDManipContentUpdatedFn)(void *ctx, float scroll_x, float scroll_y);

/** @brief Creates the per-window DirectManipulation manager.
 *  @param hwnd  Window handle to associate with DManip.
 *  @param out   Receives the created manager on success.
 *  @return S_OK on success, or a COM error code. */
HRESULT                           flux_dmanip_create(HWND hwnd, FluxDManip **out);

/** @brief Destroys the DirectManipulation manager and releases all resources. */
void                              flux_dmanip_destroy(FluxDManip *dm);

/** @brief Pumps DManip's update loop; call once per frame before reading scroll offsets.
 *
 *  Triggers OnContentUpdated callbacks for viewports that are actively panning or in inertia.
 *  @param dm  Manager to tick.
 *  @return true if any viewport is still animating (caller should keep requesting frames). */
bool                              flux_dmanip_tick(FluxDManip *dm);

/** @brief Creates a per-ScrollView DManip viewport.
 *  @param dm   Manager owning this viewport.
 *  @param out  Receives the created viewport on success.
 *  @return S_OK on success, or a COM error code. */
HRESULT                           flux_dmanip_viewport_create(FluxDManip *dm, FluxDManipViewport **out);

/** @brief Destroys a viewport and releases its DManip resources. */
void                              flux_dmanip_viewport_destroy(FluxDManipViewport *vp);

/** @brief Updates the visible viewport rect in client-space pixels (DPI-scaled).
 *
 *  DManip refuses to operate if @p w or @p h is zero or negative.
 *  @param vp       Target viewport.
 *  @param x,y,w,h  Client-space pixel bounds. */
void                              flux_dmanip_viewport_set_rect(FluxDManipViewport *vp, int x, int y, int w, int h);

/** @brief Updates the scrollable content size in client-space pixels.
 *
 *  DManip uses content_size − viewport_size as the pan limit.
 *  @param vp   Target viewport.
 *  @param w,h  Full scrollable content dimensions. */
void                              flux_dmanip_viewport_set_content_size(FluxDManipViewport *vp, int w, int h);

/** @brief Sets the DIP-to-pixel scale for unit conversion in callbacks and sync_transform.
 *
 *  The content-updated callback receives pixels; sync_transform accepts DIPs —
 *  this scale bridges them.  Default is 1.0; update on WM_DPICHANGED.
 *  @param vp     Target viewport.
 *  @param scale  DIP-to-pixel ratio (e.g. 1.25 at 125% DPI). */
void                              flux_dmanip_viewport_set_dpi_scale(FluxDManipViewport *vp, float scale);

/** @brief Hands off an active WM_POINTER contact to DManip for gesture tracking.
 *
 *  Pointer updates for this ID are consumed by DManip until the user lifts
 *  or DManip releases the contact.  Call from pointer_down on a scroll hit.
 *  @param vp          Target viewport.
 *  @param pointer_id  WM_POINTER contact identifier.
 *  @return S_OK on success, or a COM error code. */
HRESULT                           flux_dmanip_viewport_set_contact(FluxDManipViewport *vp, uint32_t pointer_id);

/** @brief Registers the content-updated callback, replacing any previous one.
 *  @param vp   Target viewport.
 *  @param cb   Callback fired on each transform update.
 *  @param ctx  Caller context passed to @p cb. */
void flux_dmanip_viewport_set_callback(FluxDManipViewport *vp, FluxDManipContentUpdatedFn cb, void *ctx);

/** @brief Pushes a scroll offset into DManip to synchronize its internal transform with the app state.
 *
 *  No-op while the viewport is actively manipulating — DManip is the source of truth in those states.
 *  @param vp        Target viewport.
 *  @param scroll_x  Horizontal scroll offset in DIPs.
 *  @param scroll_y  Vertical scroll offset in DIPs. */
void flux_dmanip_viewport_sync_transform(FluxDManipViewport *vp, float scroll_x, float scroll_y);

/** @brief Returns true while the user is touching the viewport or inertia is playing.
 *  @return true if the viewport is in RUNNING or INERTIA state. */
bool flux_dmanip_viewport_is_manipulating(FluxDManipViewport const *vp);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_DMANIP_H */
