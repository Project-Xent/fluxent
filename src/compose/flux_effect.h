/**
 * @file flux_effect.h
 * @brief In-content acrylic / blur via the WUC effect graph.
 *
 * WUC has no built-in acrylic brush that samples *app* content; you build one
 * from a `BackdropBrush` source fed through a Direct2D effect graph. cwinrt
 * projects the effect *factory* but not the Win2D-style effect *description*, so
 * the description is a hand-rolled COM object (see flux_effect.c) implementing
 * `IGraphicsEffectD2D1Interop` + the WinRT `IGraphicsEffect`/`IGraphicsEffectSource`
 * facets. This is ADR 0001 risk #1 — the one piece whose pure-C feasibility was
 * uncertain.
 *
 * NOTE: compile-verified; runtime rendering (does the compositor accept the
 * graph and blur app content) requires a GPU desktop session.
 */
#ifndef FLUX_COMPOSE_EFFECT_H
#define FLUX_COMPOSE_EFFECT_H

#include "compose/flux_compose.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Build a backdrop brush blurred by a Gaussian effect graph.
 *
 * The returned brush samples whatever is behind the visual it is assigned to
 * and blurs it — the core of in-content acrylic (tint/noise layer on top of
 * this in a later step). Owned by the caller; assign to a sprite visual and
 * release when done.
 *
 * @param c           Shared compositor.
 * @param blur_amount Gaussian standard deviation (DIPs).
 * @return A CompositionBrush (base interface), or NULL on failure.
 */
WUC_Brush *flux_effect_make_backdrop_blur(FluxCompositor *c, float blur_amount);

/**
 * @brief Build a *host*-backdrop brush blurred by a Gaussian effect graph.
 *
 * Like flux_effect_make_backdrop_blur, but the backdrop samples the desktop
 * content *behind the window* rather than the window's own content. This is the
 * correct source for a standalone top-level window (a popup / flyout / menu /
 * tooltip), whose own composition tree has nothing behind the acrylic layer.
 *
 * @param c           Shared compositor.
 * @param blur_amount Gaussian standard deviation (DIPs).
 * @return A CompositionBrush (base interface), or NULL on failure.
 */
WUC_Brush *flux_effect_make_host_backdrop_blur(FluxCompositor *c, float blur_amount);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_EFFECT_H */
