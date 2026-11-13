#ifndef FLUX_BREADCRUMB_INTERNAL_H
#define FLUX_BREADCRUMB_INTERNAL_H

#include <stdbool.h>
#include <xent/xent.h>
#include "store/flux_node_data.h"

typedef struct FluxStore             FluxStore;
typedef struct FluxWindow            FluxWindow;
typedef struct FluxTheme             FluxTheme;
typedef struct FluxText              FluxText;
typedef struct FluxInput             FluxInput;
typedef struct FluxMenu              FluxMenu;
typedef struct FluxBreadcrumbBarData FluxBreadcrumbBarData;

#define FLUX_BREADCRUMB_ITEM_H        26.0f
#define FLUX_BREADCRUMB_FONT          14.0f
#define FLUX_BREADCRUMB_PAD_H         1.0f
#define FLUX_BREADCRUMB_CHEVRON_FONT  12.0f
#define FLUX_BREADCRUMB_CHEVRON_PAD   2.0f
#define FLUX_BREADCRUMB_ELLIPSIS_PAD  3.0f
#define FLUX_BREADCRUMB_ELLIPSIS_FONT 14.0f
#define FLUX_BREADCRUMB_CHEVRON_LTR   L"\xE974"
#define FLUX_BREADCRUMB_CHEVRON_RTL   L"\xE973"
#define FLUX_BREADCRUMB_ELLIPSIS      L"\xE712"

typedef struct FluxBreadcrumbItem {
	FluxBreadcrumbBarData *bar;
	XentNodeId             node;
	int                    elem;
} FluxBreadcrumbItem;

typedef struct FluxBreadcrumbFlyoutSlot {
	FluxBreadcrumbBarData *bar;
	int                    index;
} FluxBreadcrumbFlyoutSlot;

struct FluxBreadcrumbBarData {
	XentCtx                  *ctx;
	FluxStore                *store;
	FluxWindow               *window;
	FluxText                 *text;
	FluxTheme                *theme;
	FluxInput                *input;
	XentNodeId                root;

	char const              **labels;
	int                       count;

	XentNodeId               *nodes;
	float                    *widths;
	FluxBreadcrumbFlyoutSlot *fly_slots;

	float                     chevron_cell_w;
	float                     natural_w;
	float                     last_avail;
	bool                      widths_dirty;
	bool                      arrange_dirty;

	bool                      ellipsis_rendered;
	int                       first_rendered;
	bool                      disabled;

	FluxMenu                 *flyout;

	void                      (*on_item_clicked)(void *ctx, int index);
	void                     *on_item_clicked_ctx;
};

#endif
