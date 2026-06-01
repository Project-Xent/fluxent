/**
 * @file flux_compose.h
 * @brief Windows.UI.Composition substrate for Fluxent (internal).
 *
 * Confines all WinRT projection details (cwinrt, hand-declared interop, HSTRING,
 * activation factories) behind a small C surface in fluxent vocabulary, so the
 * graphics layer and control builders never touch an `lpVtbl`.
 *
 * ## Ownership model
 *
 * - `FluxCompositor` is a thread-level singleton: the WinRT apartment, the
 *   DispatcherQueue, and the WUC `Compositor`. Acquire/release is refcounted;
 *   the underlying objects live from the first acquire to the last release.
 * - `FluxWindowTarget` is per-HWND: a `DesktopWindowTarget` plus its root visual.
 *   The root visual's content (the swap chain surface) is rebindable so device
 *   loss does not require recreating the target.
 */
#ifndef FLUX_COMPOSE_H
#define FLUX_COMPOSE_H

#include <windows.h>

#include <cwinrt/Windows.UI.Composition.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxCompositor   FluxCompositor;
typedef struct FluxWindowTarget FluxWindowTarget;

/**
 * @brief Acquire the thread-shared compositor, creating it on first call.
 *
 * Initializes the WinRT apartment (STA) and DispatcherQueue if needed, then
 * activates the WUC Compositor. Refcounted: pair every call with a release.
 *
 * @return Shared compositor, or NULL on failure.
 */
FluxCompositor                 *flux_compositor_acquire(void);

/**
 * @brief Release a reference to the shared compositor.
 *
 * The DispatcherQueue and Compositor are torn down when the last reference is
 * released. Passing NULL is safe.
 */
void                            flux_compositor_release(FluxCompositor *c);

/**
 * @brief The underlying WUC Compositor (not owned by the caller).
 */
WUC_Comp                       *flux_compositor_comp(FluxCompositor *c);

/**
 * @brief Query any composition object to its IVisual facet (new ref, or NULL).
 *
 * cwinrt's Create*Visual return the runtime class's *default* interface
 * (ISpriteVisual / IShapeVisual / IContainerVisual), which is NOT IVisual.
 * IVisual methods (Offset/Size/Clip/...) dispatch by vtable slot, so they must
 * be called on a real IVisual pointer — a raw C cast lands on the wrong method.
 * Use this to obtain the IVisual facet; release it when done.
 */
WUC_Visual                     *flux_compose_as_visual(void *composition_object);

/**
 * @brief Create a CompositionGraphicsDevice backed by a Direct2D device.
 *
 * Required for drawing surfaces (Phase B text bridge). The graphics device is
 * owned by the caller and released with flux_compositor_graphics_device_release().
 *
 * @param c          Shared compositor.
 * @param d2d_device The ID2D1Device the surfaces will render through.
 * @return New graphics device, or NULL on failure.
 */
WUC_CompositionGraphicsDevice  *flux_compositor_create_graphics_device(FluxCompositor *c, IUnknown *d2d_device);

/**
 * @brief Release a graphics device created by flux_compositor_create_graphics_device().
 */
void                            flux_compositor_graphics_device_release(WUC_CompositionGraphicsDevice *device);

/**
 * @brief Create a composition target bound to an HWND.
 *
 * The target owns a root SpriteVisual. Bind content with
 * flux_window_target_set_swapchain(). One target per HWND.
 *
 * @param c    Shared compositor.
 * @param hwnd Window to host the visual tree.
 * @return New window target, or NULL on failure.
 */
FluxWindowTarget               *flux_window_target_create(FluxCompositor *c, HWND hwnd);

/**
 * @brief Destroy a window target and its root visual. NULL is safe.
 */
void                            flux_window_target_destroy(FluxWindowTarget *t);

/**
 * @brief Point the root visual at a swap chain's content.
 *
 * Wraps the swap chain as a composition surface and assigns it as the root
 * visual's brush. Call on attach and after device loss (when the swap chain
 * object is recreated). Replaces any previous binding.
 *
 * @param t         Window target.
 * @param swapchain The IDXGISwapChain to display.
 * @return S_OK on success, or an HRESULT error.
 */
HRESULT                         flux_window_target_set_swapchain(FluxWindowTarget *t, IUnknown *swapchain);

/**
 * @brief Present a swap chain in a content child sprite over an acrylic root.
 *
 * Unlike flux_window_target_set_swapchain (which makes the swap chain the root
 * visual's own brush), this hosts the swap chain in a *child* sprite composed
 * on top of the root. The root's brush is then free to carry a backdrop /
 * acrylic material (see flux_window_target_set_backdrop_brush) that shows
 * through wherever the swap chain content is translucent. Idempotent — rebinds
 * the existing child on device loss. Used by standalone popup windows.
 *
 * @param t         Window target.
 * @param swapchain The IDXGISwapChain to display.
 * @return S_OK on success, or an HRESULT error.
 */
HRESULT                         flux_window_target_present_swapchain_child(FluxWindowTarget *t, IUnknown *swapchain);

/**
 * @brief Set the root visual's brush to an arbitrary backdrop material.
 *
 * Pairs with flux_window_target_present_swapchain_child: the brush (e.g. a host
 * backdrop blur from flux_effect) becomes the backplate behind the swap chain
 * content child. The target takes its own reference. NULL clears it.
 */
void                            flux_window_target_set_backdrop_brush(FluxWindowTarget *t, WUC_Brush *brush);

/**
 * @brief Set the root visual size in DIPs (call on resize).
 */
void                            flux_window_target_set_size(FluxWindowTarget *t, float w, float h);

/**
 * @brief The root visual of the target (not owned). Children attach here.
 */
WUC_Visual                     *flux_window_target_root(FluxWindowTarget *t);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_H */
