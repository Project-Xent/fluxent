#ifndef FLUX_SELECTOR_BAR_DATA_H
#define FLUX_SELECTOR_BAR_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxInput           FluxInput;
typedef struct FluxWindow          FluxWindow;
typedef struct FluxSelectorBarData FluxSelectorBarData;

typedef struct FluxSelectorBarItemData {
	FluxSelectorBarData *bar;
	int                  index;
	char const          *text;
	uint32_t             icon_glyph;
	bool                 disabled;
	bool                 selected;
	float                pill_t;
} FluxSelectorBarItemData;

typedef struct FluxSelectorBarData {
	XentCtx                  *ctx;
	FluxStore                *store;
	FluxInput                *input;
	FluxWindow               *window;
	XentNodeId                root;
	XentNodeId                viewport;
	XentNodeId                host;

	int                       count;
	int                       selected;

	XentNodeId               *items;
	FluxSelectorBarItemData **item_data;

	unsigned long             anim_start;
	int                       anim_item;

	void                      (*on_select)(void *ctx, int index);
	void                     *on_select_ctx;
} FluxSelectorBarData;

#endif
