/**
 * @file flux_internal.h
 * @brief Fluxent bridge internals: runtime config glue and Host event routing.
 *
 * Reconciler/runtime live in xent-kit. Built-in controls still paint through the
 * private draw table in controls/factory/flux_control_registry.h; Components and
 * HtmlView attach XentDl values replayed by XtkBackend.
 */
#ifndef FLUX_INTERNAL_H
#define FLUX_INTERNAL_H

#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_media.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_textbox.h"
#include "controls/factory/flux_init_value.h"
#include <xtk/xtk_backend.h>
#include <xtk/xtk_catalog.h>
#include <xtk/xtk_reconcile.h>

typedef struct FluxBackendCtx {
	XentCtx   *ctx;
	FluxStore *store;
	FluxApp   *app;
	XtkHost   *host;
	XtkRt     *runtime;
} FluxBackendCtx;

XtkRtCfg flux_xtk_runtime_config(FluxBackendCtx *bctx);

void     flux_input_route_activation(XtkHost *host, FluxStore *store, XentNodeId node, XtkEventCause cause);
void     flux_input_route_event(
  XtkHost *host, XentNodeId node, XtkEventId id, XtkEventPayload payload, XtkEventCause cause
);
void flux_input_route_invoke(XtkHost *host, XentNodeId node, XtkEventCause cause);

#endif
