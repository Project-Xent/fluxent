#ifndef FLUX_A11Y_UIA_H
#define FLUX_A11Y_UIA_H

#include "fluxent/flux_node_store.h"

#include <windows.h>

typedef struct FluxUiaContext {
	XentCtx   *ctx;
	XentNodeId root;
	HWND       hwnd;
	FluxStore *store;
} FluxUiaContext;

void *flux_uia_provider_create(FluxUiaContext const *info);

void  flux_uia_raisefocus(FluxUiaContext const *info, XentNodeId node);

#endif
