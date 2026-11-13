#ifndef FLUX_CHECKBOX_DATA_H
#define FLUX_CHECKBOX_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>

typedef enum FluxCheckState
{
	FLUX_CHECK_UNCHECKED,
	FLUX_CHECK_CHECKED,
	FLUX_CHECK_INDETERMINATE,
} FluxCheckState;

typedef struct FluxCheckboxData {
	char const    *label;
	XentCtx       *ctx;
	XentNodeId     node;
	FluxCheckState state;
	bool           is_three_state;
	void           (*on_change)(void *ctx, FluxCheckState new_state);
	void          *on_change_ctx;
} FluxCheckboxData;

static void inline flux_checkbox_setstate(FluxCheckboxData *cd, FluxCheckState state) {
	cd->state = state;
	if (cd->ctx)
		xent_sem_setchecked(
		  cd->ctx, cd->node,
		  state == FLUX_CHECK_CHECKED         ? 1u
		  : state == FLUX_CHECK_INDETERMINATE ? 2u
		                                      : 0u
		);
}

#endif
