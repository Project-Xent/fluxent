/**
 * @file flux_interaction.h
 * @brief Composition InteractionTracker scroll engine (ADR 0001 §4; WUC successor to DManip).
 *
 * The modern, compositor-thread equivalent of `flux_dmanip`: an InteractionTracker
 * plus a VisualInteractionSource bound to a scroll viewport's composition visual.
 * 1:1 finger/touchpad tracking, inertia, and rails run on the compositor; the UI
 * thread polls the resolved position back into FluxScrollData each frame.
 *
 * Only meaningful in the composition render path (FLUX_USE_COMPOSITION); the
 * gesture handoff needs a Windows.UI.Input.PointerPoint produced from the host's
 * WM_POINTER stream via pointer-interop, supplied by the caller.
 *
 * NOTE: compile/link-verified. Tracking feel, inertia, and the pointer handoff are
 * runtime judgments requiring a GPU session.
 */
#ifndef FLUX_INPUT_INTERACTION_H
#define FLUX_INPUT_INTERACTION_H

#include "compose/flux_compose.h"

#include <stdbool.h>
#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Per-scroll-view InteractionTracker + its visual interaction source. */
typedef struct FluxInteraction FluxInteraction;

/**
 * @brief Create a tracker driving @p scroll_visual.
 * @param c             Shared compositor.
 * @param scroll_visual Composition visual whose Position the tracker manipulates.
 * @return New engine, or NULL on failure.
 */
FluxInteraction               *flux_interaction_create(FluxCompositor *c, WUC_Visual *scroll_visual);

/** @brief Destroy the tracker and release its source. NULL-safe. */
void                           flux_interaction_destroy(FluxInteraction *it);

/** @brief Set the scrollable extent (content minus viewport) in DIPs; clamps the
 *  tracker's min/max position to [0, max]. */
void                           flux_interaction_set_extent(FluxInteraction *it, float max_x, float max_y);

/** @brief Hand an active pointer to the tracker for direct manipulation.
 *  @param pointer_point A Windows.UI.Input.PointerPoint* (WUIIN_PointerPoint), from
 *         the host's WM_POINTER stream via pointer-interop. */
HRESULT                        flux_interaction_redirect(FluxInteraction *it, void *pointer_point);

/** @brief Programmatically scroll to an absolute offset (DIPs). */
void                           flux_interaction_scroll_to(FluxInteraction *it, float x, float y);

/** @brief Poll the tracker's current position into @p out_x / @p out_y (DIPs).
 *  @return true if the position changed since the previous poll. */
bool                           flux_interaction_poll(FluxInteraction *it, float *out_x, float *out_y);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_INPUT_INTERACTION_H */
