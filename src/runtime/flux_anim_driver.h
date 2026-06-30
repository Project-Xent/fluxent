/**
 * @file flux_anim_driver.h
 * @brief Shared animation driver: one ~60Hz timer ticking all active animators.
 *
 * Controls used to each carry their own fixed-cap global array + WM_TIMER plumbing
 * (g_nav_anim/g_tab_anim/...), duplicating the same start/remove/tick machinery and
 * silently dropping the Nth simultaneous animation past the cap. This is the single
 * owner of that concern (Doctrine #6): register a per-frame step keyed by an opaque
 * ctx, and it is ticked until the step reports it is done. The registry grows as
 * needed, so there is no silent cap.
 */
#ifndef FLUX_ANIM_DRIVER_H
#define FLUX_ANIM_DRIVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Per-frame animation step.
 * @param ctx     The opaque token passed to flux_anim_register.
 * @param now_ms  GetTickCount() sampled once per tick (shared by all steps this frame).
 * @return true while still animating; false to auto-unregister this animator.
 */
typedef bool (*FluxAnimStep)(void *ctx, unsigned long now_ms);

/**
 * @brief Register @p ctx to be ticked at ~60Hz. Idempotent per ctx (updates the
 *        step if already registered). Starts the shared timer on the first animator.
 */
void flux_anim_register(void *ctx, FluxAnimStep step);

/**
 * @brief Stop ticking @p ctx. Safe to call if not registered. Stops the shared timer
 *        once the last animator is gone. Call from a control's destroy path.
 */
void flux_anim_unregister(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
