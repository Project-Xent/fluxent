#ifndef FLUX_RADIO_BUTTONS_DATA_H
#define FLUX_RADIO_BUTTONS_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <xent/xent.h>

typedef struct FluxInput            FluxInput;
typedef struct FluxRadioButtonsData FluxRadioButtonsData;

typedef struct FluxRadioItemCtx {
	FluxRadioButtonsData *group;
	int                   index;
} FluxRadioItemCtx;

typedef struct FluxRadioButtonsData {
	XentCtx          *ctx;
	FluxStore        *store;
	FluxInput        *input;
	XentNodeId        root;

	int               count;
	int               selected;
	int               columns;
	bool              disabled;

	XentNodeId       *items;
	FluxRadioItemCtx *item_ctx;

	void              (*on_select)(void *ctx, int index);
	void             *on_select_ctx;
} FluxRadioButtonsData;

#endif
