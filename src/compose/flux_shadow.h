/**
 * @file flux_shadow.h
 * @brief Composition drop shadow for elevation.
 *
 * Replaces hand-drawn elevation gradients with a real `DropShadow` composited
 * off-thread.
 */
#ifndef FLUX_COMPOSE_SHADOW_H
#define FLUX_COMPOSE_SHADOW_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a drop shadow and attach it to a sprite visual.
 *
 * @param c      Shared compositor.
 * @param sprite Sprite visual to receive the shadow.
 * @param color  Shadow color (alpha controls strength).
 * @param blur   Blur radius (DIPs).
 * @param dx     Horizontal offset (DIPs).
 * @param dy     Vertical offset (DIPs).
 * @return The shadow (owned by caller; release via flux_shadow_destroy), or NULL.
 */
WUC_DropShadow *
flux_shadow_apply(FluxCompositor *c, WUC_Sprite *sprite, FluxColor color, float blur, float dx, float dy);

/** @brief Release a shadow created by flux_shadow_apply(). NULL is safe. */
void flux_shadow_destroy(WUC_DropShadow *shadow);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_SHADOW_H */
