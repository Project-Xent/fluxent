#ifndef FLUX_DMANIP_H
#define FLUX_DMANIP_H

/* ============================================================================
 * flux_dmanip — pure-C wrapper around DirectManipulation (DManip)
 *
 * DirectManipulation is the low-level compositor-aware gesture / inertia
 * engine that underpins WinUI's ScrollViewer.  It provides:
 *   • 1:1 finger tracking during pan
 *   • Physical inertia curves after flick
 *   • Edge rubber-band (over-pan + spring-back)
 *   • Axis rails (lock to X or Y when the gesture is near-axial)
 *   • Multi-touch handoff via pointer contact IDs
 *
 * This wrapper runs DManip in CPU-threaded (host-update) mode: we read the
 * content transform from OnContentUpdated() callbacks and write it back to
 * FluxScrollData.scroll_x/y.  The compositor still animates smoothly thanks
 * to DManip's internal timer; no custom inertia math needed.
 *
 * A later pass may migrate to compositor-threaded mode (per-ScrollView
 * IDCompositionVisual with IDirectManipulationCompositor) for zero-UI-thread
 * scrolling latency.
 * ============================================================================ */

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Opaque handles. */
typedef struct FluxDManip         FluxDManip;         /* per-window */
typedef struct FluxDManipViewport FluxDManipViewport; /* per-ScrollView */

/* Fired whenever DManip's transform changes (user pan, inertia tick, rubber
 * band, etc.).  Always invoked on the UI thread, inside flux_dmanip_tick(). */
typedef void                      (*FluxDManipContentUpdatedFn)(void *ctx, float scroll_x, float scroll_y);

/* ── Manager (one per window) ────────────────────────────────────────── */

HRESULT                           flux_dmanip_create(HWND hwnd, FluxDManip **out);
void                              flux_dmanip_destroy(FluxDManip *dm);

/* Pump DManip's update loop — call once per render frame *before* reading
 * any scroll offsets.  Triggers OnContentUpdated callbacks for viewports
 * that are actively panning or inertia-running.
 * Returns true if any viewport is still animating (caller should keep
 * requesting frames). */
bool                              flux_dmanip_tick(FluxDManip *dm);

/* ── Viewport (one per XENT_CONTROL_SCROLL node) ─────────────────────── */

HRESULT                           flux_dmanip_viewport_create(FluxDManip *dm, FluxDManipViewport **out);
void                              flux_dmanip_viewport_destroy(FluxDManipViewport *vp);

/* Update the visible rect (client-space pixels, DPI-scaled).  Call when
 * layout changes.  DManip refuses operation if w<=0 or h<=0. */
void                              flux_dmanip_viewport_set_rect(FluxDManipViewport *vp, int x, int y, int w, int h);

/* Update content size (client-space pixels).  Must cover the full scrollable
 * extent; DManip uses content_size − viewport_size as the pan limit. */
void                              flux_dmanip_viewport_set_content_size(FluxDManipViewport *vp, int w, int h);

/* Tell the viewport the current DIP→pixel scale (e.g. 1.25 at 125% DPI).
 * Required for correct unit conversion on the two "soft" boundaries:
 *   • the content-updated callback (DManip reports pixels, app wants DIPs)
 *   • sync_transform           (app passes DIPs, DManip wants pixels)
 * set_rect/set_content_size still take pixels directly.
 * Default 1.0; update whenever WM_DPICHANGED fires. */
void                              flux_dmanip_viewport_set_dpi_scale(FluxDManipViewport *vp, float scale);

/* Hand off an active WM_POINTER contact to DManip.  From this point,
 * pointer updates for this ID are consumed by DManip and no longer reach
 * WM_POINTERUPDATE — until the user lifts or DManip releases the contact.
 * Call from pointer_down when the hit is inside a scrollable node. */
HRESULT                           flux_dmanip_viewport_set_contact(FluxDManipViewport *vp, uint32_t pointer_id);

/* Register the content-updated callback.  Replaces any previous one. */
void flux_dmanip_viewport_set_callback(FluxDManipViewport *vp, FluxDManipContentUpdatedFn cb, void *ctx);

/* Push a scroll offset into DManip so its internal transform matches
 * the app's current scroll state.  Use this after the app changes
 * scroll_x/y via a non-touch path (mouse wheel, scrollbar drag,
 * programmatic ScrollTo) so the next touch-pan starts from the
 * correct origin instead of snapping back to zero.
 *
 * No-op while the viewport is actively manipulating (RUNNING / INERTIA)
 * \u2014 DManip is the source of truth in those states. */
void flux_dmanip_viewport_sync_transform(FluxDManipViewport *vp, float scroll_x, float scroll_y);

/* True while the user is touching the viewport or inertia is playing.
 * Callers use this to decide whether to defer programmatic offset
 * changes until the gesture settles. */
bool flux_dmanip_viewport_is_manipulating(FluxDManipViewport const *vp);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_DMANIP_H */
