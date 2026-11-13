#ifndef FLUX_ITEMS_VIEW_DATA_H
#define FLUX_ITEMS_VIEW_DATA_H

#include "flux_list_view_data.h"

typedef struct FluxItemsViewData {
	FluxListViewData   list;
	XtkItemsViewLayout layout;
	float              item_spacing;
} FluxItemsViewData;

#endif
