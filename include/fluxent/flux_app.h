/**
 * @file flux_app.h
 * @brief Fluxent XTK bridge — re-exports xtk.h and provides the
 * platform-specific flux_app_run() entry point for Windows.
 */
#ifndef FLUXENT_FLUX_APP_H
#define FLUXENT_FLUX_APP_H

#include <xtk/xtk.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxAppConfig FluxAppConfig;

/**
 * @brief Create the app window and drive update/view until it closes.
 *
 * Each frame: queued control messages are drained through @p update, then
 * (only if anything changed) @p view rebuilds the element tree and the
 * reconciler applies the diff to retained controls. Returns the app's exit
 * code.
 */
int flux_run(FluxAppConfig const *cfg, void *model, XtkUpdateFn update, XtkViewFn view);

#ifdef __cplusplus
}
#endif

#endif
