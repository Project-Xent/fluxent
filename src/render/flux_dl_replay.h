#ifndef FLUX_RENDER_DL_REPLAY_H
#define FLUX_RENDER_DL_REPLAY_H

#include <stdbool.h>
#include <xtk/xtk_backend.h>
#include <xent/xent_display.h>

typedef struct FluxBackendCtx FluxBackendCtx;
typedef struct FluxRenderCtx  FluxRenderCtx;

XtkBackend                   *flux_xtk_backend_create(FluxBackendCtx *bctx, XtkHost *host);
void                          flux_xtk_backend_destroy(XtkBackend *backend);
void                          flux_xtk_setrc(XtkBackend *backend, FluxRenderCtx const *rc);
void                          flux_xtk_regimage(XtkBackend *backend, XentResourceId id, char const *path);

/** @brief Replay a display list onto an arbitrary FluxRenderCtx (popup surfaces). */
bool                          flux_xtk_replay_rc(FluxRenderCtx const *rc, XentDl const *list);

#endif
