#ifndef FLUX_COMPOSE_RENDER_H
#define FLUX_COMPOSE_RENDER_H

#include "compose/flux_compose.h"
#include "fluxent/flux_node_store.h"

typedef struct FluxRenderCtx       FluxRenderCtx;
typedef struct FluxControlRegistry FluxControlRegistry;
typedef struct XtkBackend          XtkBackend;

typedef struct FluxComposeRender   FluxComposeRender;

FluxComposeRender                 *flux_compose_render_create(
  FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root, FluxControlRegistry const *registry,
  XtkBackend *backend
);

void flux_compose_render_destroy(FluxComposeRender *r);

void flux_compose_render_frame(
  FluxComposeRender *r, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxRenderCtx const *rc_tmpl
);

bool flux_compose_redirect_scroll(FluxComposeRender *r, XentNodeId node, uint32_t pointer_id);

#endif
