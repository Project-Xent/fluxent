#ifndef FLUX_ACCESS_KEYS_H
#define FLUX_ACCESS_KEYS_H

#include "a11y/flux_uia.h"
#include "fluxent/flux_input.h"

#define FLUX_ACCESS_KEY_CAP 7

typedef struct FluxRenderCtx FluxRenderCtx;

typedef struct FluxAccessKeys {
	wchar_t prefix [FLUX_ACCESS_KEY_CAP];
	int     length;
	HWND    surface;
	bool    visible;
	bool    alt_pending;
} FluxAccessKeys;

bool flux_access_keys_key(FluxAccessKeys *keys, FluxKey key, bool down, bool repeated);
bool flux_access_keys_char(FluxAccessKeys *keys, FluxUiaContext const *info, FluxInput *input, wchar_t ch, bool alt);
void flux_access_keys_show(FluxAccessKeys *keys, FluxUiaContext const *info, FluxInput const *input);
void flux_access_keys_paint(
  FluxAccessKeys const *keys, FluxUiaContext const *info, FluxInput const *input, FluxRenderCtx const *rc
);

#endif
