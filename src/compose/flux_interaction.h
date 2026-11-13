#ifndef FLUX_COMPOSE_INTERACTION_H
#define FLUX_COMPOSE_INTERACTION_H

#include "compose/flux_compose.h"

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

typedef struct FluxInteraction FluxInteraction;

FluxInteraction               *flux_interaction_create(FluxCompositor *c, WUC_Visual *scroll_visual);

void                           flux_interaction_destroy(FluxInteraction *it);

HRESULT                        flux_interaction_bind_offset(FluxInteraction *it, WUC_Visual *target_visual);

void                           flux_interaction_set_extent(FluxInteraction *it, float max_x, float max_y);

HRESULT                        flux_interaction_redirect(FluxInteraction *it, void *pointer_point);

HRESULT                        flux_interaction_redirectpid(FluxInteraction *it, uint32_t pointer_id);

void                           flux_interaction_scroll_to(FluxInteraction *it, float x, float y);

bool                           flux_interaction_poll(FluxInteraction *it, float *out_x, float *out_y);

#endif
