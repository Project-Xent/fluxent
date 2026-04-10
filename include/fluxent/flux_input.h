#ifndef FLUX_INPUT_H
#define FLUX_INPUT_H

#include "flux_component_data.h"
#include "flux_node_store.h"
#include "flux_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxHitResult {
    XentNodeId    node;
    FluxNodeData *data;
    FluxRect      bounds;
    FluxPoint     local;
} FluxHitResult;

typedef struct FluxInput FluxInput;

FluxInput    *flux_input_create(XentContext *ctx, FluxNodeStore *store);
void          flux_input_destroy(FluxInput *input);

FluxHitResult flux_input_hit_test(FluxInput *input, XentNodeId root,
                                  float px, float py);

void          flux_input_pointer_down(FluxInput *input, XentNodeId root,
                                      float px, float py);

void          flux_input_pointer_up(FluxInput *input, XentNodeId root,
                                    float px, float py);

void          flux_input_pointer_move(FluxInput *input, XentNodeId root,
                                      float px, float py);

XentNodeId    flux_input_get_focused(const FluxInput *input);
XentNodeId    flux_input_get_hovered(const FluxInput *input);
XentNodeId    flux_input_get_pressed(const FluxInput *input);

#ifdef __cplusplus
}
#endif

#endif