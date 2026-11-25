#ifndef FLUX_A11Y_UIA_H
#define FLUX_A11Y_UIA_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <windows.h>

typedef struct FluxText FluxText;

typedef struct FluxUiaContext {
	XentCtx   *ctx;
	XentNodeId root;
	HWND       hwnd;
	FluxStore *store;
	FluxText  *text;
} FluxUiaContext;

void                       *flux_uia_provider_create(FluxUiaContext const *info);

void                        flux_uia_raisefocus(FluxUiaContext const *info, XentNodeId node, int sub);
int                         flux_uia_focussub(FluxUiaContext const *info, XentNodeId node);

typedef struct FluxUiaTip   FluxUiaTip;
typedef struct FluxUiaWatch FluxUiaWatch;

FluxUiaWatch               *flux_uia_watch_create(void);
void                        flux_uia_watch_destroy(FluxUiaWatch *watch);
void                        flux_uia_watch_flush(FluxUiaContext const *info);
void                        flux_uia_watch_pump(FluxUiaWatch *watch, FluxUiaContext const *info);

FluxUiaTip                 *flux_uia_tip_create(void);
void                        flux_uia_tip_release(FluxUiaTip *tip);
void                        flux_uia_tip_update(FluxUiaTip *tip, HWND hwnd, char const *text);
LRESULT                     flux_uia_tip_object(void *ctx, HWND hwnd, WPARAM wp, LPARAM lp);
void                        flux_uia_tip_raise(FluxUiaTip *tip, bool opened);

#endif
