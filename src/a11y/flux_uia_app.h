#ifndef FLUX_UIA_APP_H
#define FLUX_UIA_APP_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <stdint.h>

enum
{
	FLUX_A11Y_NAME = 0,
	FLUX_A11Y_ID,
	FLUX_A11Y_HELP,
	FLUX_A11Y_DESCRIPTION,
	FLUX_A11Y_ITEM_STATUS,
	FLUX_A11Y_ITEM_TYPE,
	FLUX_A11Y_CONTROL_TYPE,
	FLUX_A11Y_ACCELERATOR,
	FLUX_A11Y_ACCESS_KEY,
	FLUX_A11Y_LANDMARK_NAME,
	FLUX_A11Y_LABELED_BY,
	FLUX_A11Y_DESCRIBED_BY,
	FLUX_A11Y_FLOWS_TO,
	FLUX_A11Y_FLOWS_FROM,
	FLUX_A11Y_CONTROLLED,
	FLUX_A11Y_TEXTS,
};

enum
{
	FLUX_A11Y_LANDMARK = 0,
	FLUX_A11Y_LIVE,
	FLUX_A11Y_HEADING,
	FLUX_A11Y_VIEW,
	FLUX_A11Y_REQUIRED,
	FLUX_A11Y_VALID,
	FLUX_A11Y_PERIPHERAL,
	FLUX_A11Y_DIALOG,
	FLUX_A11Y_POSITION,
	FLUX_A11Y_SIZE,
	FLUX_A11Y_LEVEL,
	FLUX_A11Y_CULTURE,
	FLUX_A11Y_NUMS,
};

typedef struct FluxUiaProps {
	char    *text [FLUX_A11Y_TEXTS];
	int32_t  num [FLUX_A11Y_NUMS];
	uint32_t set;
} FluxUiaProps;

void        flux_uia_settext(FluxStore *store, XentNodeId node, int slot, char const *text);
void        flux_uia_setnum(FluxStore *store, XentNodeId node, int slot, int32_t value);
char const *flux_uia_apptext(FluxStore *store, XentNodeId node, int slot);
bool        flux_uia_appnum(FluxStore *store, XentNodeId node, int slot, int32_t *out);
void        flux_uia_props_free(FluxUiaProps *props);

#endif
