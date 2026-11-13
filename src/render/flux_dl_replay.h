#ifndef FLUX_RENDER_DL_REPLAY_H
#define FLUX_RENDER_DL_REPLAY_H

#include <stdbool.h>
#include <xtk/xtk_backend.h>
#include <xent/xent_display.h>

typedef struct FluxBackendCtx FluxBackendCtx;
typedef struct FluxRenderCtx  FluxRenderCtx;

XtkBackend                   *flux_xtk_backend_create(FluxBackendCtx *bctx, XtkHost *host);
FluxBackendCtx               *flux_xtk_backendctx(XtkBackend *backend);
void                          flux_xtk_backend_destroy(XtkBackend *backend);
void                          flux_xtk_setrc(XtkBackend *backend, FluxRenderCtx const *rc);

bool                          flux_xtk_replay_rc(FluxRenderCtx const *rc, XentDl const *list);

#endif
