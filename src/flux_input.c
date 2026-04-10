#include "fluxent/flux_input.h"

#include <stdlib.h>
#include <string.h>

struct FluxInput {
    XentContext   *ctx;
    FluxNodeStore *store;
    XentNodeId     hovered;
    XentNodeId     pressed;
    XentNodeId     focused;
    FluxRect       pressed_bounds;
};

/* Hit-test using absolute layout coordinates.
   scroll_x/scroll_y track cumulative scroll offset from ancestor scroll
   containers.  xent_get_layout_rect returns absolute positions, so the
   screen position of a node is  rect.x - scroll_x. */
static FluxHitResult hit_test_recursive(XentContext *ctx, XentNodeId node,
                                        float px, float py,
                                        float scroll_x, float scroll_y) {
    XentRect rect = {0};
    xent_get_layout_rect(ctx, node, &rect);

    float ax = rect.x - scroll_x;
    float ay = rect.y - scroll_y;

    if (px < ax || px >= ax + rect.width || py < ay || py >= ay + rect.height)
        return (FluxHitResult){0};

    float child_scroll_x = scroll_x;
    float child_scroll_y = scroll_y;
    FluxNodeData *nd = (FluxNodeData *)xent_get_userdata(ctx, node);
    if (nd && xent_get_control_type(ctx, node) == XENT_CONTROL_SCROLL) {
        FluxScrollData *sd = (FluxScrollData *)nd->component_data;
        if (sd) { child_scroll_x += sd->scroll_x; child_scroll_y += sd->scroll_y; }
    }

    FluxHitResult best = {0};
    XentNodeId child = xent_get_first_child(ctx, node);
    while (child != XENT_NODE_INVALID) {
        FluxHitResult r = hit_test_recursive(ctx, child, px, py,
                                             child_scroll_x, child_scroll_y);
        if (r.node != XENT_NODE_INVALID) best = r;
        child = xent_get_next_sibling(ctx, child);
    }
    if (best.node != XENT_NODE_INVALID) return best;

    return (FluxHitResult){
        .node   = node,
        .data   = nd,
        .bounds = { ax, ay, rect.width, rect.height },
        .local  = { px - ax, py - ay },
    };
}

FluxInput *flux_input_create(XentContext *ctx, FluxNodeStore *store) {
    if (!ctx || !store) {
        return NULL;
    }
    FluxInput *input = (FluxInput *)calloc(1, sizeof(*input));
    if (!input) {
        return NULL;
    }
    input->ctx     = ctx;
    input->store   = store;
    input->hovered = XENT_NODE_INVALID;
    input->pressed = XENT_NODE_INVALID;
    input->focused = XENT_NODE_INVALID;
    return input;
}

void flux_input_destroy(FluxInput *input) {
    free(input);
}

FluxHitResult flux_input_hit_test(FluxInput *input, XentNodeId root, float px, float py) {
    if (!input || root == XENT_NODE_INVALID) {
        return (FluxHitResult){0};
    }
    return hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
}

void flux_input_pointer_move(FluxInput *input, XentNodeId root, float px, float py) {
    if (!input || root == XENT_NODE_INVALID) {
        return;
    }

    FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
    XentNodeId new_hovered = hit.node;

    if (new_hovered != input->hovered) {
        if (input->hovered != XENT_NODE_INVALID) {
            FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
            if (old) old->state.hovered = 0;
        }
        if (new_hovered != XENT_NODE_INVALID && hit.data) {
            hit.data->state.hovered = 1;
        }
        input->hovered = new_hovered;
    }

    /* During a drag, forward pointer_move to the pressed node so that
       sliders (and other drag-interactive controls) keep receiving
       position updates even when the cursor leaves the control bounds.
       We intentionally do NOT fire on_pointer_move for plain hover —
       hover visual feedback is handled via state.hovered instead. */
    if (input->pressed != XENT_NODE_INVALID) {
        FluxNodeData *pressed_nd = flux_node_store_get(input->store, input->pressed);
        if (pressed_nd && pressed_nd->on_pointer_move) {
            float local_x = px - input->pressed_bounds.x;
            float local_y = py - input->pressed_bounds.y;
            pressed_nd->on_pointer_move(pressed_nd->on_pointer_move_ctx,
                                        local_x, local_y);
        }
    }
}

void flux_input_pointer_down(FluxInput *input, XentNodeId root, float px, float py) {
    if (!input || root == XENT_NODE_INVALID) {
        return;
    }

    FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);

    if (hit.node != XENT_NODE_INVALID && hit.data) {
        hit.data->state.pressed = 1;
        input->pressed = hit.node;
        input->pressed_bounds = hit.bounds;

        /* Fire on_pointer_move immediately so controls like sliders can
           respond to click-to-position (not only drag). */
        if (hit.data->on_pointer_move)
            hit.data->on_pointer_move(hit.data->on_pointer_move_ctx,
                                      hit.local.x, hit.local.y);
    }

    if (hit.node != input->focused) {
        if (input->focused != XENT_NODE_INVALID) {
            FluxNodeData *old = flux_node_store_get(input->store, input->focused);
            if (old) {
                old->state.focused = 0;
                if (old->on_blur) old->on_blur(old->on_blur_ctx);
            }
        }
        /* Don't set state.focused for pointer-initiated focus — the focus
           ring should only appear for keyboard navigation.  We still track
           focus internally for blur/focus lifecycle callbacks. */
        if (hit.node != XENT_NODE_INVALID && hit.data) {
            if (hit.data->on_focus) hit.data->on_focus(hit.data->on_focus_ctx);
        }
        input->focused = hit.node;
    }
}

void flux_input_pointer_up(FluxInput *input, XentNodeId root, float px, float py) {
    if (!input || root == XENT_NODE_INVALID) {
        return;
    }

    if (input->pressed != XENT_NODE_INVALID) {
        FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
        if (nd) {
            nd->state.pressed = 0;

            FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
            if (hit.node == input->pressed && nd->on_click) {
                nd->on_click(nd->on_click_ctx);
            }
        }
        input->pressed = XENT_NODE_INVALID;
    }
}

XentNodeId flux_input_get_focused(const FluxInput *input) {
    return input ? input->focused : XENT_NODE_INVALID;
}

XentNodeId flux_input_get_hovered(const FluxInput *input) {
    return input ? input->hovered : XENT_NODE_INVALID;
}

XentNodeId flux_input_get_pressed(const FluxInput *input) {
    return input ? input->pressed : XENT_NODE_INVALID;
}