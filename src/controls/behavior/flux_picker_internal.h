#ifndef FLUX_PICKER_INTERNAL_H
#define FLUX_PICKER_INTERNAL_H

#include "controls/behavior/flux_picker_flyout.h"
#include "controls/data/flux_picker_data.h"

struct FluxPickerOps {
	float width;
	void  (*face)(FluxPickerData *pd);
	void  (*fill)(FluxPickerData *pd, FluxPickerFlyout *fly);
	void  (*load)(FluxPickerData *pd);
	void  (*select)(FluxPickerData *pd, int column, int index);
	int   (*accept)(FluxPickerData *pd);
};

bool flux_picker_setup(
  FluxPickerData *pd, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxControlType type, float width
);

void flux_picker_refresh(FluxPickerData *pd);

void flux_picker_destroy(void *component_data);

void flux_picker_snapshot(FluxPickerSnapshot *out, FluxPickerData const *pd);

#endif
