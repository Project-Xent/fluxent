#include "fluxent/flux_input.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

struct FluxInput {
    XentContext   *ctx;
    FluxNodeStore *store;
    XentNodeId     hovered;
    XentNodeId     pressed;
    XentNodeId     focused;
    FluxRect       pressed_bounds;
    /* Double/triple click detection */
    int            click_count;
    DWORD          last_click_time;
    float          last_click_x;
    float          last_click_y;
    XentNodeId     last_click_node;
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
    input->hovered         = XENT_NODE_INVALID;
    input->pressed         = XENT_NODE_INVALID;
    input->focused         = XENT_NODE_INVALID;
    input->click_count     = 0;
    input->last_click_node = XENT_NODE_INVALID;
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
            if (old) {
                old->state.hovered = 0;
                old->hover_local_x = -1.0f;
                old->hover_local_y = -1.0f;
            }
        }
        if (new_hovered != XENT_NODE_INVALID && hit.data) {
            hit.data->state.hovered = 1;
        }
        input->hovered = new_hovered;
    }

    /* Update hover local coordinates on the currently hovered node */
    if (input->hovered != XENT_NODE_INVALID) {
        FluxNodeData *hnd = flux_node_store_get(input->store, input->hovered);
        if (hnd) {
            XentRect hrect = {0};
            xent_get_layout_rect(input->ctx, input->hovered, &hrect);
            hnd->hover_local_x = px - hrect.x;
            hnd->hover_local_y = py - hrect.y;
        }
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

    /* NumberBox spin-button intercept: ONLY block spin area clicks.
       Spin clicks synthesize VK_UP/VK_DOWN via on_key — no pressed, no focus.
       X (delete) button clicks are NOT intercepted here — they pass through
       to on_pointer_down normally so the control gets focused and tb_on_pointer_down
       handles the clear-text logic. */
    if (hit.node != XENT_NODE_INVALID && hit.data) {
        XentControlType nb_ct = xent_get_control_type(input->ctx, hit.node);
        if (nb_ct == XENT_CONTROL_NUMBER_BOX) {
            bool spin_inline = xent_get_semantic_expanded(input->ctx, hit.node);
            if (spin_inline) {
                XentRect nb_rect = {0};
                xent_get_layout_rect(input->ctx, hit.node, &nb_rect);
                float spin_start = nb_rect.width - 76.0f;
                if (hit.local.x >= spin_start) {
                    bool up = (hit.local.x < spin_start + 40.0f);
                    if (hit.data->on_key) {
                        hit.data->on_key(hit.data->on_key_ctx,
                                         up ? 0x26u /* VK_UP */ : 0x28u /* VK_DOWN */,
                                         true);
                    }
                    return; /* consumed — no pressed, no focus, no on_pointer_down */
                }
            }
        }
    }

    /* Double/triple click detection */
    DWORD now_ms = GetTickCount();
    DWORD dbl_time = GetDoubleClickTime();
    float dx = px - input->last_click_x;
    float dy = py - input->last_click_y;
    float dist2 = dx * dx + dy * dy;

    if (hit.node == input->last_click_node &&
        (now_ms - input->last_click_time) < dbl_time &&
        dist2 < 25.0f) {  /* ~5px tolerance */
        input->click_count++;
        if (input->click_count > 3) input->click_count = 3;
    } else {
        input->click_count = 1;
    }
    input->last_click_time = now_ms;
    input->last_click_x = px;
    input->last_click_y = py;
    input->last_click_node = hit.node;

    if (hit.node != XENT_NODE_INVALID && hit.data) {
        hit.data->state.pressed = 1;
        input->pressed = hit.node;
        input->pressed_bounds = hit.bounds;

        /* Fire on_pointer_move immediately so controls like sliders can
           respond to click-to-position (not only drag). */
        if (hit.data->on_pointer_move)
            hit.data->on_pointer_move(hit.data->on_pointer_move_ctx,
                                      hit.local.x, hit.local.y);

        if (hit.data->on_pointer_down)
            hit.data->on_pointer_down(hit.data->on_pointer_down_ctx,
                                      hit.local.x, hit.local.y, input->click_count);
    }

    if (hit.node != input->focused) {
        if (input->focused != XENT_NODE_INVALID) {
            FluxNodeData *old = flux_node_store_get(input->store, input->focused);
            if (old) {
                old->state.focused = 0;
                if (old->on_blur) old->on_blur(old->on_blur_ctx);
            }
        }
        if (hit.node != XENT_NODE_INVALID && hit.data) {
            /* Text inputs always need visual focus for caret/accent line */
            XentControlType ct = xent_get_control_type(input->ctx, hit.node);
            if (ct == XENT_CONTROL_TEXT_INPUT)
                hit.data->state.focused = 1;
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

            /* PasswordBox: clear reveal on mouse release (press-to-reveal behavior) */
            if (xent_get_control_type(input->ctx, input->pressed) == XENT_CONTROL_PASSWORD_BOX) {
                xent_set_semantic_checked(input->ctx, input->pressed, 0);
            }

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

void flux_input_key_down(FluxInput *input, unsigned int vk) {
    if (!input || input->focused == XENT_NODE_INVALID) return;
    FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
    if (nd && nd->on_key)
        nd->on_key(nd->on_key_ctx, vk, true);
}

void flux_input_char(FluxInput *input, wchar_t ch) {
    if (!input || input->focused == XENT_NODE_INVALID) return;
    FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
    if (nd && nd->on_char)
        nd->on_char(nd->on_char_ctx, ch);
}

int flux_input_get_click_count(const FluxInput *input) {
    return input ? input->click_count : 1;
}

void flux_input_ime_composition(FluxInput *input, const wchar_t *text,
                                uint32_t length, uint32_t cursor) {
    if (!input || input->focused == XENT_NODE_INVALID) return;
    FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
    if (nd && nd->on_ime_composition)
        nd->on_ime_composition(nd->on_ime_composition_ctx, text, length, cursor);
}

void flux_input_ime_end(FluxInput *input) {
    if (!input || input->focused == XENT_NODE_INVALID) return;
    FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
    if (nd && nd->on_ime_composition)
        nd->on_ime_composition(nd->on_ime_composition_ctx, NULL, 0, 0);
}

void flux_input_context_menu(FluxInput *input, XentNodeId root,
                             float px, float py, HWND hwnd) {
    if (!input) return;
    /* If the focused node supports context menu, delegate to it */
    if (input->focused != XENT_NODE_INVALID) {
        FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
        if (nd && nd->on_context_menu) {
            nd->on_context_menu(nd->on_context_menu_ctx, px, py);
            return;
        }
    }
}

void flux_input_scroll(FluxInput *input, XentNodeId root, float px, float py, float delta) {
    if (!input || root == XENT_NODE_INVALID) return;

    /* Hit-test to find the node under the cursor */
    FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
    if (hit.node == XENT_NODE_INVALID) return;

    /* Walk up the tree to find a handler */
    XentNodeId node = hit.node;
    while (node != XENT_NODE_INVALID) {
        XentControlType ct = xent_get_control_type(input->ctx, node);

        /* NumberBox: scroll wheel steps value when focused (WinUI OnNumberBoxScroll) */
        if (ct == XENT_CONTROL_NUMBER_BOX) {
            FluxNodeData *nd = flux_node_store_get(input->store, node);
            if (nd && nd->state.focused && nd->on_key) {
                /* Simulate Up/Down arrow key press */
                if (delta > 0)
                    nd->on_key(nd->on_key_ctx, 0x26 /* VK_UP */, true);
                else if (delta < 0)
                    nd->on_key(nd->on_key_ctx, 0x28 /* VK_DOWN */, true);
            }
            return; /* handled */
        }

        /* ScrollView: adjust scroll offset */
        if (ct == XENT_CONTROL_SCROLL) {
            FluxNodeData *nd = flux_node_store_get(input->store, node);
            if (nd && nd->component_data) {
                FluxScrollData *sd = (FluxScrollData *)nd->component_data;
                float scroll_amount = -delta * 48.0f;
                sd->scroll_y += scroll_amount;

                if (sd->scroll_y < 0.0f) sd->scroll_y = 0.0f;

                XentRect rect = {0};
                xent_get_layout_rect(input->ctx, node, &rect);
                float max_y = sd->content_h - rect.height;
                if (max_y < 0.0f) max_y = 0.0f;
                if (sd->scroll_y > max_y) sd->scroll_y = max_y;
            }
            return; /* handled */
        }

        node = xent_get_parent(input->ctx, node);
    }
}

/* ---- Focus Navigation ---- */

/* DFS collect all visible, focusable nodes */
static void collect_focusable(XentContext *ctx, XentNodeId node,
                              XentNodeId *out, uint32_t *count, uint32_t max_count) {
    if (node == XENT_NODE_INVALID || *count >= max_count) return;

    /* Only include alive, focusable, and enabled nodes */
    if (xent_get_focusable(ctx, node) && xent_get_semantic_enabled(ctx, node)) {
        /* Check visibility: node should have non-zero layout rect */
        XentRect rect = {0};
        xent_get_layout_rect(ctx, node, &rect);
        if (rect.width > 0.0f && rect.height > 0.0f) {
            out[*count] = node;
            (*count)++;
        }
    }

    /* Recurse into children */
    XentNodeId child = xent_get_first_child(ctx, node);
    while (child != XENT_NODE_INVALID && *count < max_count) {
        collect_focusable(ctx, child, out, count, max_count);
        child = xent_get_next_sibling(ctx, child);
    }
}

/* Comparison function for sorting by tab_index (stable by insertion order) */
static XentContext *s_sort_ctx = NULL;

static int compare_tab_index(const void *a, const void *b) {
    XentNodeId na = *(const XentNodeId *)a;
    XentNodeId nb = *(const XentNodeId *)b;
    int32_t ta = xent_get_tab_index(s_sort_ctx, na);
    int32_t tb = xent_get_tab_index(s_sort_ctx, nb);

    /* tab_index == 0 means "natural order" (treat as large value) */
    int32_t ea = (ta == 0) ? 0x7FFFFFFF : ta;
    int32_t eb = (tb == 0) ? 0x7FFFFFFF : tb;

    if (ea != eb) return (ea < eb) ? -1 : 1;
    /* Stable: preserve document order (lower id first) */
    return (na < nb) ? -1 : (na > nb) ? 1 : 0;
}

void flux_input_tab(FluxInput *input, XentNodeId root, bool shift) {
    if (!input || root == XENT_NODE_INVALID) return;

    /* Collect focusable nodes */
    XentNodeId focusable[512];
    uint32_t count = 0;
    collect_focusable(input->ctx, root, focusable, &count, 512);

    if (count == 0) return;

    /* Filter out negative tab_index (they are programmatic-only, skip Tab) */
    XentNodeId tab_order[512];
    uint32_t tab_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        int32_t ti = xent_get_tab_index(input->ctx, focusable[i]);
        if (ti >= 0) {
            tab_order[tab_count++] = focusable[i];
        }
    }
    if (tab_count == 0) return;

    /* Sort by tab_index */
    s_sort_ctx = input->ctx;
    qsort(tab_order, tab_count, sizeof(XentNodeId), compare_tab_index);
    s_sort_ctx = NULL;

    /* Find current focused node in the sorted list */
    int current_idx = -1;
    for (uint32_t i = 0; i < tab_count; i++) {
        if (tab_order[i] == input->focused) {
            current_idx = (int)i;
            break;
        }
    }

    /* Calculate next index */
    int next_idx;
    if (current_idx < 0) {
        next_idx = shift ? (int)tab_count - 1 : 0;
    } else if (shift) {
        next_idx = (current_idx - 1 + (int)tab_count) % (int)tab_count;
    } else {
        next_idx = (current_idx + 1) % (int)tab_count;
    }

    XentNodeId next_node = tab_order[next_idx];

    /* Blur old */
    if (input->focused != XENT_NODE_INVALID) {
        FluxNodeData *old = flux_node_store_get(input->store, input->focused);
        if (old) {
            old->state.focused = 0;
            if (old->on_blur) old->on_blur(old->on_blur_ctx);
        }
    }

    /* Focus new */
    input->focused = next_node;
    FluxNodeData *nd = flux_node_store_get(input->store, next_node);
    if (nd) {
        nd->state.focused = 1;
        if (nd->on_focus) nd->on_focus(nd->on_focus_ctx);
    }
}

void flux_input_arrow(FluxInput *input, XentNodeId root, int direction) {
    if (!input || root == XENT_NODE_INVALID) return;
    if (input->focused == XENT_NODE_INVALID) return;

    /* Get parent of focused node to find siblings */
    XentNodeId parent = xent_get_parent(input->ctx, input->focused);
    if (parent == XENT_NODE_INVALID) return;

    /* Collect focusable siblings */
    XentNodeId siblings[128];
    uint32_t sib_count = 0;
    XentNodeId child = xent_get_first_child(input->ctx, parent);
    while (child != XENT_NODE_INVALID && sib_count < 128) {
        if (xent_get_focusable(input->ctx, child) &&
            xent_get_semantic_enabled(input->ctx, child)) {
            siblings[sib_count++] = child;
        }
        child = xent_get_next_sibling(input->ctx, child);
    }

    if (sib_count <= 1) return;

    /* Find current in siblings */
    int cur = -1;
    for (uint32_t i = 0; i < sib_count; i++) {
        if (siblings[i] == input->focused) {
            cur = (int)i;
            break;
        }
    }
    if (cur < 0) return;

    /* Determine direction: left/up = -1, right/down = +1 */
    int delta = 0;
    if (direction == 0x25 || direction == 0x26) delta = -1;  /* VK_LEFT, VK_UP */
    if (direction == 0x27 || direction == 0x28) delta = +1;  /* VK_RIGHT, VK_DOWN */
    if (delta == 0) return;

    int next = (cur + delta + (int)sib_count) % (int)sib_count;
    XentNodeId next_node = siblings[next];

    /* Blur old */
    FluxNodeData *old = flux_node_store_get(input->store, input->focused);
    if (old) {
        old->state.focused = 0;
        if (old->on_blur) old->on_blur(old->on_blur_ctx);
    }

    /* Focus new */
    input->focused = next_node;
    FluxNodeData *nd = flux_node_store_get(input->store, next_node);
    if (nd) {
        nd->state.focused = 1;
        if (nd->on_focus) nd->on_focus(nd->on_focus_ctx);
    }
}

void flux_input_activate(FluxInput *input) {
    if (!input || input->focused == XENT_NODE_INVALID) return;

    FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
    if (nd && nd->on_click) {
        nd->on_click(nd->on_click_ctx);
    }
}

void flux_input_escape(FluxInput *input) {
    if (!input) return;

    if (input->focused != XENT_NODE_INVALID) {
        FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
        if (nd) {
            nd->state.focused = 0;
            if (nd->on_blur) nd->on_blur(nd->on_blur_ctx);
        }
        input->focused = XENT_NODE_INVALID;
    }
}

void flux_input_set_focus(FluxInput *input, XentNodeId node) {
    if (!input) return;

    /* Blur old */
    if (input->focused != XENT_NODE_INVALID && input->focused != node) {
        FluxNodeData *old = flux_node_store_get(input->store, input->focused);
        if (old) {
            old->state.focused = 0;
            if (old->on_blur) old->on_blur(old->on_blur_ctx);
        }
    }

    /* Focus new */
    input->focused = node;
    if (node != XENT_NODE_INVALID) {
        FluxNodeData *nd = flux_node_store_get(input->store, node);
        if (nd) {
            nd->state.focused = 1;
            if (nd->on_focus) nd->on_focus(nd->on_focus_ctx);
        }
    }
}