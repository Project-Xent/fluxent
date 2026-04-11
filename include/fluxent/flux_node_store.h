#ifndef FLUX_NODE_STORE_H
#define FLUX_NODE_STORE_H

#include "flux_types.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxNodeVisuals {
    FluxColor background;
    FluxColor border_color;
    float     corner_radius;
    float     border_width;
    float     opacity;
} FluxNodeVisuals;

typedef struct FluxNodeState {
    uint8_t enabled  : 1;
    uint8_t visible  : 1;
    uint8_t hovered  : 1;
    uint8_t pressed  : 1;
    uint8_t focused  : 1;
    uint8_t dirty    : 1;
} FluxNodeState;

typedef struct FluxNodeData {
    XentNodeId        node_id;
    FluxNodeVisuals   visuals;
    FluxNodeState     state;
    void             *component_data;

    void            (*on_click)(void *ctx);
    void             *on_click_ctx;
    void            (*on_pointer_move)(void *ctx, float x, float y);
    void             *on_pointer_move_ctx;
    void            (*on_focus)(void *ctx);
    void             *on_focus_ctx;
    void            (*on_blur)(void *ctx);
    void             *on_blur_ctx;
    void            (*on_pointer_down)(void *ctx, float x, float y, int click_count);
    void             *on_pointer_down_ctx;
    void            (*on_key)(void *ctx, unsigned int vk, bool down);
    void             *on_key_ctx;
    void            (*on_char)(void *ctx, wchar_t ch);
    void             *on_char_ctx;
    void            (*on_ime_composition)(void *ctx, const wchar_t *text, uint32_t len, uint32_t cursor);
    void             *on_ime_composition_ctx;
    void            (*on_context_menu)(void *ctx, float x, float y);
    void             *on_context_menu_ctx;
} FluxNodeData;

typedef struct FluxNodeStore FluxNodeStore;

FluxNodeStore *flux_node_store_create(uint32_t initial_capacity);
void           flux_node_store_destroy(FluxNodeStore *store);

FluxNodeData  *flux_node_store_get(FluxNodeStore *store, XentNodeId id);
FluxNodeData  *flux_node_store_get_or_create(FluxNodeStore *store, XentNodeId id);
void           flux_node_store_remove(FluxNodeStore *store, XentNodeId id);
uint32_t       flux_node_store_count(const FluxNodeStore *store);

void           flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx);

#ifdef __cplusplus
}
#endif

#endif