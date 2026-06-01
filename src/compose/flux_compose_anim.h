/**
 * @file flux_compose_anim.h
 * @brief Off-thread composition animations (keyframe + WinUI easing).
 *
 * Replaces the UI-thread `FluxTween` model: instead of recomputing a value
 * every frame, a control sets a target and the DWM compositor interpolates it
 * on its own thread (ADR 0001 §4). A `FluxAnimKit` caches the reusable WinUI
 * easing curves and starts scalar/color keyframe animations against any
 * composition object property (Offset/Scale/Opacity/RotationAngle on a visual,
 * Color on a brush, etc.).
 *
 * NOTE: built and compile-verified; wiring control state transitions onto these
 * helpers (and deleting the old tween loop) is Phase C, gated on GPU runtime
 * validation of timing and feel.
 */
#ifndef FLUX_COMPOSE_ANIM_H
#define FLUX_COMPOSE_ANIM_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxAnimKit FluxAnimKit;

/**
 * @brief WinUI-style easing curves, realized as cached cubic-bezier functions.
 */
typedef enum FluxEaseId
{
	FLUX_EASE_LINEAR = 0, /**< No easing function (linear interpolation). */
	FLUX_EASE_STANDARD,   /**< Default point-to-point decelerate. */
	FLUX_EASE_ACCELERATE, /**< Exit/entrance acceleration. */
	FLUX_EASE_DECELERATE, /**< Entrance deceleration. */
	FLUX_EASE_COUNT,
} FluxEaseId;

/**
 * @brief Create an animation kit bound to the shared compositor.
 */
FluxAnimKit *flux_anim_kit_create(FluxCompositor *c);

/**
 * @brief Destroy an animation kit and its cached easing functions. NULL safe.
 */
void         flux_anim_kit_destroy(FluxAnimKit *kit);

/**
 * @brief Animate a scalar property to a target value over a duration.
 *
 * @param kit     Animation kit.
 * @param target  Any composition object (visual, brush, ...) — QI'd internally.
 * @param prop    Property name (e.g. L"Opacity", L"RotationAngle").
 * @param to      Target value.
 * @param seconds Duration in seconds.
 * @param ease    Easing curve.
 * @return S_OK on success, or an HRESULT error.
 */
HRESULT
flux_anim_scalar(FluxAnimKit *kit, void *target, wchar_t const *prop, float to, double seconds, FluxEaseId ease);

/**
 * @brief Animate a color property to a target color over a duration.
 *
 * @param kit     Animation kit.
 * @param target  Any composition object exposing a color property (e.g. a
 *                CompositionColorBrush) — QI'd internally.
 * @param prop    Property name (e.g. L"Color").
 * @param to      Target color.
 * @param seconds Duration in seconds.
 * @param ease    Easing curve.
 * @return S_OK on success, or an HRESULT error.
 */
HRESULT
flux_anim_color(FluxAnimKit *kit, void *target, wchar_t const *prop, FluxColor to, double seconds, FluxEaseId ease);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_ANIM_H */
