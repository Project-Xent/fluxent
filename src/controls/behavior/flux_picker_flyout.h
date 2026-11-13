#ifndef FLUX_PICKER_FLYOUT_H
#define FLUX_PICKER_FLYOUT_H

#include "controls/data/flux_picker_data.h"

typedef struct FluxPickerFlyoutDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	FluxInput  *input;
	FluxWindow *window;
	XentNodeId  overlay;
	float       width;
	bool        time;
	void        (*on_accept)(void *ctx);
	void        (*on_dismiss)(void *ctx);
	void        (*on_select)(void *ctx, int column, int index);
	void       *userdata;
} FluxPickerFlyoutDesc;

typedef struct FluxPickerColumnSpec {
	char const *const *items;
	int                count;
	int                selected;
	float              weight;
	bool               loop;
	bool               left;
} FluxPickerColumnSpec;

FluxPickerFlyout *flux_pflyout_create(FluxPickerFlyoutDesc const *desc);
void              flux_pflyout_destroy(FluxPickerFlyout *fly);

int               flux_pflyout_addcolumn(FluxPickerFlyout *fly, FluxPickerColumnSpec const *spec);

void flux_pflyout_setcolumn(FluxPickerFlyout *fly, int column, char const *const *items, int count, int selected);

void flux_pflyout_show(FluxPickerFlyout *fly, FluxRect const *target);
void flux_pflyout_hide(FluxPickerFlyout *fly);
void flux_pflyout_refresh(FluxPickerFlyout const *fly);

#endif
