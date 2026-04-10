#include "fluxent/flux_engine.h"
#include "flux_render_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct FluxCommandBuffer {
    FluxRenderCommand *cmds;
    uint32_t           count;
    uint32_t           capacity;
} FluxCommandBuffer;

struct FluxEngine {
    FluxNodeStore      *store;
    FluxCommandBuffer   commands;
};

static bool flux_command_buffer_push(FluxCommandBuffer *buf, const FluxRenderCommand *cmd) {
    if (buf->count == buf->capacity) {
        uint32_t new_cap = buf->capacity ? buf->capacity * 2 : 256;
        FluxRenderCommand *new_cmds = (FluxRenderCommand *)realloc(buf->cmds, sizeof(FluxRenderCommand) * new_cap);
        if (!new_cmds) {
            return false;
        }
        buf->cmds = new_cmds;
        buf->capacity = new_cap;
    }
    buf->cmds[buf->count++] = *cmd;
    return true;
}

static FluxControlState flux_compute_control_state(const FluxNodeData *nd) {
    FluxControlState s = {0};
    if (!nd) {
        s.enabled = 1;
        return s;
    }
    s.hovered = nd->state.hovered;
    s.pressed = nd->state.pressed;
    s.focused = nd->state.focused;
    s.enabled = nd->state.enabled;
    return s;
}

typedef struct CollectFrame {
    XentNodeId         node;
    float              abs_x;
    float              abs_y;
    bool               main_emitted;
    bool               is_scroll;
    FluxRenderSnapshot snapshot;
    FluxControlState   state;
    XentNodeId         current_child;
} CollectFrame;

static void collect_commands(FluxEngine *eng, XentContext *ctx, XentNodeId root) {
    uint32_t stack_cap = 64;
    uint32_t stack_top = 0;
    CollectFrame *stack = (CollectFrame *)malloc(sizeof(CollectFrame) * stack_cap);
    if (!stack) return;

    /* Push root frame */
    CollectFrame root_frame;
    memset(&root_frame, 0, sizeof(root_frame));
    root_frame.node = root;
    root_frame.abs_x = 0.0f;
    root_frame.abs_y = 0.0f;
    root_frame.main_emitted = false;
    root_frame.current_child = XENT_NODE_INVALID;
    stack[stack_top++] = root_frame;

    while (stack_top > 0) {
        CollectFrame *frame = &stack[stack_top - 1];

        if (!frame->main_emitted) {
            /* First visit: compute layout, build snapshot, emit Main command */
            XentRect rect = {0};
            xent_get_layout_rect(ctx, frame->node, &rect);

            frame->abs_x = rect.x;
            frame->abs_y = rect.y;

            FluxNodeData *nd = (FluxNodeData *)xent_get_userdata(ctx, frame->node);

            flux_snapshot_build(&frame->snapshot, ctx, frame->node, nd);
            frame->state = flux_compute_control_state(nd);

            XentControlType ct = xent_get_control_type(ctx, frame->node);
            frame->is_scroll = (ct == XENT_CONTROL_SCROLL);

            /* Emit Main command */
            FluxRenderCommand cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.snapshot = frame->snapshot;
            cmd.bounds.x = frame->abs_x;
            cmd.bounds.y = frame->abs_y;
            cmd.bounds.w = rect.width;
            cmd.bounds.h = rect.height;
            cmd.state = frame->state;
            cmd.phase = FLUX_PHASE_MAIN;
            cmd.clip_action = FLUX_CLIP_NONE;
            flux_command_buffer_push(&eng->commands, &cmd);

            /* If scroll view, emit PushClip command */
            if (frame->is_scroll) {
                FluxRenderCommand clip_cmd;
                memset(&clip_cmd, 0, sizeof(clip_cmd));
                clip_cmd.bounds.x = frame->abs_x;
                clip_cmd.bounds.y = frame->abs_y;
                clip_cmd.bounds.w = rect.width;
                clip_cmd.bounds.h = rect.height;
                clip_cmd.phase = FLUX_PHASE_MAIN;
                clip_cmd.clip_action = FLUX_CLIP_PUSH;
                if (nd && nd->component_data) {
                    FluxScrollData *sd = (FluxScrollData *)nd->component_data;
                    clip_cmd.scroll_x = sd->scroll_x;
                    clip_cmd.scroll_y = sd->scroll_y;
                }
                flux_command_buffer_push(&eng->commands, &clip_cmd);
            }

            frame->current_child = xent_get_first_child(ctx, frame->node);
            frame->main_emitted = true;
            continue;
        }

        if (frame->current_child != XENT_NODE_INVALID) {
            /* There's a child to process — push a new frame for it */
            XentNodeId child = frame->current_child;
            frame->current_child = xent_get_next_sibling(ctx, child);

            /* Grow stack if needed */
            if (stack_top == stack_cap) {
                uint32_t new_cap = stack_cap * 2;
                CollectFrame *new_stack = (CollectFrame *)realloc(stack, sizeof(CollectFrame) * new_cap);
                if (!new_stack) break;
                stack = new_stack;
                stack_cap = new_cap;
                /* Reacquire frame pointer after realloc */
                frame = &stack[stack_top - 1];
            }

            /* Scroll offset is handled by D2D transform, not position accumulation.
               Children get their absolute position directly from xent_get_layout_rect. */
            CollectFrame child_frame;
            memset(&child_frame, 0, sizeof(child_frame));
            child_frame.node = child;
            child_frame.abs_x = 0.0f;
            child_frame.abs_y = 0.0f;
            child_frame.main_emitted = false;
            child_frame.current_child = XENT_NODE_INVALID;
            stack[stack_top++] = child_frame;
            continue;
        }

        /* All children done — emit PopClip (if scroll) and Overlay, then pop */
        if (frame->is_scroll) {
            FluxRenderCommand pop_cmd;
            memset(&pop_cmd, 0, sizeof(pop_cmd));
            pop_cmd.phase = FLUX_PHASE_MAIN;
            pop_cmd.clip_action = FLUX_CLIP_POP;
            flux_command_buffer_push(&eng->commands, &pop_cmd);
        }

        /* Emit Overlay command */
        {
            XentRect rect = {0};
            xent_get_layout_rect(ctx, frame->node, &rect);

            FluxRenderCommand overlay_cmd;
            memset(&overlay_cmd, 0, sizeof(overlay_cmd));
            overlay_cmd.snapshot = frame->snapshot;
            overlay_cmd.bounds.x = frame->abs_x;
            overlay_cmd.bounds.y = frame->abs_y;
            overlay_cmd.bounds.w = rect.width;
            overlay_cmd.bounds.h = rect.height;
            overlay_cmd.state = frame->state;
            overlay_cmd.phase = FLUX_PHASE_OVERLAY;
            overlay_cmd.clip_action = FLUX_CLIP_NONE;
            flux_command_buffer_push(&eng->commands, &overlay_cmd);
        }

        stack_top--;
    }

    free(stack);
}

FluxEngine *flux_engine_create(FluxNodeStore *store) {
    if (!store) {
        return NULL;
    }
    FluxEngine *eng = (FluxEngine *)calloc(1, sizeof(*eng));
    if (!eng) {
        return NULL;
    }
    eng->store = store;
    return eng;
}

void flux_engine_destroy(FluxEngine *eng) {
    if (!eng) {
        return;
    }
    free(eng->commands.cmds);
    free(eng);
}

void flux_engine_collect(FluxEngine *eng, XentContext *ctx, XentNodeId root) {
    if (!eng || !ctx || root == XENT_NODE_INVALID) {
        return;
    }
    eng->commands.count = 0;
    collect_commands(eng, ctx, root);
}

uint32_t flux_engine_command_count(const FluxEngine *eng) {
    return eng ? eng->commands.count : 0;
}

const FluxRenderCommand *flux_engine_command_at(const FluxEngine *eng, uint32_t index) {
    if (!eng || index >= eng->commands.count) {
        return NULL;
    }
    return &eng->commands.cmds[index];
}

/* ----- Execute phase ----- */

#define MAX_TRANSFORM_DEPTH 16

void flux_engine_execute(const FluxEngine *eng, const FluxRenderContext *rc) {
    if (!eng || !rc) return;

    D2D1_MATRIX_3X2_F transform_stack[MAX_TRANSFORM_DEPTH];
    uint32_t transform_top = 0;

    for (uint32_t i = 0; i < eng->commands.count; i++) {
        const FluxRenderCommand *cmd = &eng->commands.cmds[i];

        if (cmd->clip_action == FLUX_CLIP_PUSH) {
            /* Save current transform */
            D2D1_MATRIX_3X2_F current;
            ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &current);
            if (transform_top < MAX_TRANSFORM_DEPTH) {
                transform_stack[transform_top++] = current;
            }

            /* Push axis-aligned clip */
            D2D1_RECT_F clip = { cmd->bounds.x, cmd->bounds.y,
                                 cmd->bounds.x + cmd->bounds.w,
                                 cmd->bounds.y + cmd->bounds.h };
            ID2D1RenderTarget_PushAxisAlignedClip(
                FLUX_RT(rc), &clip,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            /* Build translation matrix for scroll offset */
            D2D1_MATRIX_3X2_F translate;
            translate._11 = 1.0f; translate._12 = 0.0f;
            translate._21 = 0.0f; translate._22 = 1.0f;
            translate._31 = -cmd->scroll_x; translate._32 = -cmd->scroll_y;

            /* Multiply: combined = translate * current */
            D2D1_MATRIX_3X2_F combined;
            combined._11 = translate._11 * current._11 + translate._12 * current._21;
            combined._12 = translate._11 * current._12 + translate._12 * current._22;
            combined._21 = translate._21 * current._11 + translate._22 * current._21;
            combined._22 = translate._21 * current._12 + translate._22 * current._22;
            combined._31 = translate._31 * current._11 + translate._32 * current._21 + current._31;
            combined._32 = translate._31 * current._12 + translate._32 * current._22 + current._32;
            ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
            continue;
        }

        if (cmd->clip_action == FLUX_CLIP_POP) {
            /* Restore previous transform and pop clip */
            if (transform_top > 0) {
                ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &transform_stack[--transform_top]);
            } else {
                D2D1_MATRIX_3X2_F identity;
                identity._11 = 1.0f; identity._12 = 0.0f;
                identity._21 = 0.0f; identity._22 = 1.0f;
                identity._31 = 0.0f; identity._32 = 0.0f;
                ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &identity);
            }
            ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
            continue;
        }

        if (cmd->phase == FLUX_PHASE_MAIN) {
            flux_dispatch_render(rc, &cmd->snapshot, &cmd->bounds, &cmd->state);
        } else {
            flux_dispatch_render_overlay(rc, &cmd->snapshot, &cmd->bounds);
        }
    }
}