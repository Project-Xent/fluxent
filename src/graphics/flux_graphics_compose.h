/**
 * @file flux_graphics_compose.h
 * @brief Composition-mode accessors on FluxGraphics (internal).
 *
 * Kept out of the public `flux_graphics.h` so that header stays free of cwinrt /
 * Windows.UI.Composition types. Used by the app to drive the retained
 * composition render path when `FLUX_USE_COMPOSITION` is set.
 */
#ifndef FLUX_GRAPHICS_COMPOSE_H
#define FLUX_GRAPHICS_COMPOSE_H

#include "fluxent/flux_graphics.h"
#include "compose/flux_compose.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief True when the retained-composition path is active for this context. */
bool                           flux_graphics_composition_enabled(FluxGraphics const *gfx);

/** @brief The thread-shared compositor (not owned). */
FluxCompositor                *flux_graphics_compositor(FluxGraphics *gfx);

/** @brief The root visual hosting the reconciled tree in composition mode (not owned). */
WUC_Visual                    *flux_graphics_composition_root(FluxGraphics *gfx);

/** @brief The graphics device for per-node surfaces, created lazily (not owned). */
WUC_CompositionGraphicsDevice *flux_graphics_graphics_device(FluxGraphics *gfx);

/**
 * @brief Give a top-level window a host-backdrop acrylic backplate (popup mode).
 *
 * In composition mode, makes the window's composition root sample the desktop
 * behind it through a Gaussian blur, and moves the swap chain content into a
 * child sprite composed on top. The window's own painting must leave its
 * background translucent for the acrylic to show through. Idempotent; re-arms
 * after device loss. No-op (returns false) when composition is disabled.
 *
 * @param gfx  Graphics bound to the popup window.
 * @param blur Gaussian standard deviation (DIPs).
 * @return true when the acrylic backplate is active.
 */
bool                           flux_graphics_enable_window_acrylic(FluxGraphics *gfx, float blur);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_GRAPHICS_COMPOSE_H */
