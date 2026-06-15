/**
 * @file flux_scroll.h
 * @brief Off-thread inertial scrolling via InteractionTracker.
 *
 * The no-compromise answer to drag/inertia/rubber-band: an `InteractionTracker`
 * fed by a `VisualInteractionSource` runs on the compositor thread, so scrolling
 * stays responsive even when the UI thread is busy. The tracker's `Position`
 * drives the scrolled content's `Offset` via an expression animation (the
 * binding + gesture tuning are the runtime step on a GPU/input session).
 */
#ifndef FLUX_COMPOSE_SCROLL_H
#define FLUX_COMPOSE_SCROLL_H

#include "compose/flux_compose.h"

#include <cwinrt/Windows.UI.Composition.Interactions.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxScrollTracker FluxScrollTracker;

/**
 * @brief Create a vertical-scroll tracker driven by a source visual's input.
 *
 * @param c             Shared compositor.
 * @param source_visual The visual whose touchpad/wheel input drives scrolling
 *                      (typically the scroll viewport's visual).
 * @return New tracker, or NULL on failure.
 */
FluxScrollTracker               *flux_scroll_tracker_create(FluxCompositor *c, WUC_Visual *source_visual);

/** @brief Destroy the tracker and its source. NULL is safe. */
void                             flux_scroll_tracker_destroy(FluxScrollTracker *t);

/** @brief Set the maximum vertical scroll offset (content height minus viewport). */
void                             flux_scroll_tracker_set_max_y(FluxScrollTracker *t, float max_y);

/** @brief The underlying InteractionTracker (not owned) — bind content Offset to its Position. */
WUICOIN_InteractionTracker      *flux_scroll_tracker_handle(FluxScrollTracker *t);

/**
 * @brief Drive a content visual's `Offset` from the tracker `Position` off-thread.
 *
 * Starts an expression animation `Vector3(0, -tracker.Position.Y, 0)` on the
 * content visual, so the compositor scrolls it without the UI thread.
 */
void                             flux_scroll_tracker_bind_offset(FluxScrollTracker *t, WUC_Visual *content);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_SCROLL_H */
