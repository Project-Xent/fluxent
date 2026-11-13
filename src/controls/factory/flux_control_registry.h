#ifndef FLUX_CONTROL_REGISTRY_H
#define FLUX_CONTROL_REGISTRY_H

#include "fluxent/flux_types.h"
#include "fluxent/flux_control_type.h"
#include "render/flux_engine.h"

#include <xent/xent_display.h>

typedef struct FluxControlRegistry FluxControlRegistry;

typedef XentDl                    *(*FluxControlRecordFn)(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *local_bounds, FluxControlState const *state
);

typedef struct FluxControlRenderer {
	FluxControlRecordFn record;
	FluxControlRecordFn record_overlay;
} FluxControlRenderer;

FluxControlRegistry *flux_control_registry_create(void);
void                 flux_control_registry_destroy(FluxControlRegistry *reg);
void flux_registry_set_record(FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record);
void flux_registry_set_overlay(FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record_overlay);
FluxControlRenderer const *flux_control_registry_get(FluxControlRegistry const *reg, FluxControlType type);
void                       flux_register_builtins(FluxControlRegistry *reg);

#endif
