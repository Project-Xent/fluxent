#ifndef FLUXENT_FLUX_HOST_H
#define FLUXENT_FLUX_HOST_H

#include <xtk/xtk_host.h>
#include <xtk/xtk_reconcile.h>
#include <xtk/xtk_backend.h>
#include <xtk/xtk_event.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_component.h>
#include <xent/xent.h>
#include <xent/xent_display.h>

typedef struct FluxApp   FluxApp;

typedef struct FluxStore FluxStore;

XtkHost                 *flux_host_create(void);
XtkHost                 *flux_host_adopt(XentCtx *ctx, FluxStore *store, XentNodeId root);
void                     flux_host_bindapp(XtkHost *host, FluxApp *app);
void                     flux_host_destroy(XtkHost *host);
FluxStore               *flux_host_store(XtkHost *host);
XentCtx                 *flux_host_context(XtkHost *host);

uint32_t                 flux_host_invalidation(XtkHost const *host, XtkCtrl control);
uint32_t                 flux_host_takeinvalid(XtkHost *host);
bool                     flux_host_setfocus(XtkHost *host, XtkCtrl control);
XtkCtrl                  flux_host_focus(XtkHost const *host);
uint32_t                 flux_host_route(XtkHost *host, XtkEvent *event);
XtkSubId
flux_host_subscribe(XtkHost *host, XtkCtrl target, XtkEventId event, uint32_t phases, XtkEventFn fn, void *userdata);
void        flux_host_unsubscribe(XtkHost *host, XtkSubId id);

void        flux_app_sethost(FluxApp *app, XtkHost *host, XtkBackend *backend);
XtkHost    *flux_app_attachhost(FluxApp *app);
XtkHost    *flux_app_host(FluxApp *app);
XtkBackend *flux_app_backend(FluxApp *app);

bool        flux_host_registercomp(XtkHost *host, XtkCompType const *type);
bool        flux_host_setproviders(XtkHost *host, XtkProviderDesc const *providers, uint32_t count);
XtkResource flux_image_respath(char const *path);

#endif
