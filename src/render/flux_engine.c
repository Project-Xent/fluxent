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
	FluxNodeStore    *store;
	FluxCommandBuffer commands;
};

static bool flux_command_buffer_push(FluxCommandBuffer *buf, FluxRenderCommand const *cmd) {
	if (buf->count == buf->capacity) {
		uint32_t           new_cap  = buf->capacity ? buf->capacity * 2 : 256;
		FluxRenderCommand *new_cmds = ( FluxRenderCommand * ) realloc(buf->cmds, sizeof(FluxRenderCommand) * new_cap);
		if (!new_cmds) return false;
		buf->cmds     = new_cmds;
		buf->capacity = new_cap;
	}
	buf->cmds [buf->count++] = *cmd;
	return true;
}

static FluxControlState flux_compute_control_state(FluxNodeData const *nd) {
	FluxControlState s = {0};
	if (!nd) {
		s.enabled = 1;
		return s;
	}
	s.hovered      = nd->state.hovered;
	s.pressed      = nd->state.pressed;
	s.focused      = nd->state.focused;
	s.enabled      = nd->state.enabled;
	s.pointer_type = nd->state.pointer_type;
	return s;
}

typedef struct CollectFrame {
	XentNodeId         node;
	float              abs_x;
	float              abs_y;
	bool               main_emitted;
	bool               is_scroll;
	float              scroll_off_x;
	float              scroll_off_y;
	float              viewport_w;
	float              viewport_h;
	FluxRenderSnapshot snapshot;
	FluxControlState   state;
	XentNodeId         current_child;
} CollectFrame;

static CollectFrame collect_root_frame(XentNodeId root) {
	CollectFrame frame;
	memset(&frame, 0, sizeof(frame));
	frame.node          = root;
	frame.current_child = XENT_NODE_INVALID;
	return frame;
}

static FluxRenderCommand
collect_make_draw_command(CollectFrame const *frame, XentRect const *rect, FluxCommandPhase phase) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.snapshot    = frame->snapshot;
	cmd.bounds.x    = frame->abs_x;
	cmd.bounds.y    = frame->abs_y;
	cmd.bounds.w    = rect->width;
	cmd.bounds.h    = rect->height;
	cmd.state       = frame->state;
	cmd.phase       = phase;
	cmd.clip_action = FLUX_CLIP_NONE;
	return cmd;
}

static void collect_emit_scroll_clip(FluxEngine *eng, CollectFrame *frame, XentRect const *rect) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.bounds.x        = frame->abs_x;
	cmd.bounds.y        = frame->abs_y;
	cmd.bounds.w        = rect->width;
	cmd.bounds.h        = rect->height;
	cmd.phase           = FLUX_PHASE_MAIN;
	cmd.clip_action     = FLUX_CLIP_PUSH;
	cmd.scroll_x        = frame->snapshot.scroll_x;
	cmd.scroll_y        = frame->snapshot.scroll_y;
	frame->scroll_off_x = frame->snapshot.scroll_x;
	frame->scroll_off_y = frame->snapshot.scroll_y;
	frame->viewport_w   = rect->width;
	frame->viewport_h   = rect->height;
	flux_command_buffer_push(&eng->commands, &cmd);
}

static void collect_emit_main(FluxEngine *eng, XentContext *ctx, CollectFrame *frame) {
	XentRect rect = {0};
	xent_get_layout_rect(ctx, frame->node, &rect);

	frame->abs_x     = rect.x;
	frame->abs_y     = rect.y;

	FluxNodeData *nd = ( FluxNodeData * ) xent_get_userdata(ctx, frame->node);
	flux_snapshot_build(&frame->snapshot, ctx, frame->node, nd);
	frame->state          = flux_compute_control_state(nd);
	frame->is_scroll      = frame->snapshot.type == XENT_CONTROL_SCROLL;

	FluxRenderCommand cmd = collect_make_draw_command(frame, &rect, FLUX_PHASE_MAIN);
	flux_command_buffer_push(&eng->commands, &cmd);
	if (frame->is_scroll) collect_emit_scroll_clip(eng, frame, &rect);

	frame->current_child = xent_get_first_child(ctx, frame->node);
	frame->main_emitted  = true;
}

static bool collect_child_visible(XentContext *ctx, CollectFrame const *frame, XentNodeId child) {
	if (!frame->is_scroll) return true;

	XentRect child_rect = {0};
	xent_get_layout_rect(ctx, child, &child_rect);

	float vis_left   = frame->abs_x + frame->scroll_off_x;
	float vis_top    = frame->abs_y + frame->scroll_off_y;
	float vis_right  = vis_left + frame->viewport_w;
	float vis_bottom = vis_top + frame->viewport_h;

	if (child_rect.x + child_rect.width < vis_left) return false;
	if (child_rect.x > vis_right) return false;
	if (child_rect.y + child_rect.height < vis_top) return false;
	return child_rect.y <= vis_bottom;
}

static bool collect_grow_stack(CollectFrame **stack, uint32_t *stack_cap) {
	uint32_t      new_cap   = *stack_cap * 2;
	CollectFrame *new_stack = ( CollectFrame * ) realloc(*stack, sizeof(CollectFrame) * new_cap);
	if (!new_stack) return false;
	*stack     = new_stack;
	*stack_cap = new_cap;
	return true;
}

static bool collect_push_child(CollectFrame **stack, uint32_t *stack_top, uint32_t *stack_cap, XentNodeId child) {
	if (*stack_top == *stack_cap && !collect_grow_stack(stack, stack_cap)) return false;
	(*stack) [(*stack_top)++] = collect_root_frame(child);
	return true;
}

static bool collect_visit_child(CollectFrame **stack, uint32_t *stack_top, uint32_t *stack_cap, XentContext *ctx) {
	CollectFrame *frame = &(*stack) [*stack_top - 1];
	if (frame->current_child == XENT_NODE_INVALID) return false;

	XentNodeId child     = frame->current_child;
	frame->current_child = xent_get_next_sibling(ctx, child);
	if (!collect_child_visible(ctx, frame, child)) return true;

	collect_push_child(stack, stack_top, stack_cap, child);
	return true;
}

static void collect_emit_finish(FluxEngine *eng, XentContext *ctx, CollectFrame const *frame) {
	if (frame->is_scroll) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP;
		flux_command_buffer_push(&eng->commands, &pop_cmd);
	}

	XentRect rect = {0};
	xent_get_layout_rect(ctx, frame->node, &rect);
	FluxRenderCommand overlay_cmd = collect_make_draw_command(frame, &rect, FLUX_PHASE_OVERLAY);
	flux_command_buffer_push(&eng->commands, &overlay_cmd);
}

static void collect_commands(FluxEngine *eng, XentContext *ctx, XentNodeId root) {
	uint32_t      stack_cap = 64;
	uint32_t      stack_top = 0;
	CollectFrame *stack     = ( CollectFrame * ) malloc(sizeof(CollectFrame) * stack_cap);
	if (!stack) return;

	stack [stack_top++] = collect_root_frame(root);
	while (stack_top > 0) {
		CollectFrame *frame = &stack [stack_top - 1];
		if (!frame->main_emitted) {
			collect_emit_main(eng, ctx, frame);
			continue;
		}
		if (collect_visit_child(&stack, &stack_top, &stack_cap, ctx)) continue;
		collect_emit_finish(eng, ctx, frame);
		stack_top--;
	}

	free(stack);
}

FluxEngine *flux_engine_create(FluxNodeStore *store) {
	if (!store) return NULL;
	FluxEngine *eng = ( FluxEngine * ) calloc(1, sizeof(*eng));
	if (!eng) return NULL;
	eng->store = store;
	return eng;
}

void flux_engine_destroy(FluxEngine *eng) {
	if (!eng) return;
	free(eng->commands.cmds);
	free(eng);
}

void flux_engine_collect(FluxEngine *eng, XentContext *ctx, XentNodeId root) {
	if (!eng || !ctx || root == XENT_NODE_INVALID) return;
	eng->commands.count = 0;
	collect_commands(eng, ctx, root);
}

uint32_t                 flux_engine_command_count(FluxEngine const *eng) { return eng ? eng->commands.count : 0; }

FluxRenderCommand const *flux_engine_command_at(FluxEngine const *eng, uint32_t index) {
	if (!eng || index >= eng->commands.count) return NULL;
	return &eng->commands.cmds [index];
}

#define MAX_TRANSFORM_DEPTH 16

static D2D1_MATRIX_3X2_F flux_identity_matrix(void) {
	D2D1_MATRIX_3X2_F m;
	m._11 = 1.0f;
	m._12 = 0.0f;
	m._21 = 0.0f;
	m._22 = 1.0f;
	m._31 = 0.0f;
	m._32 = 0.0f;
	return m;
}

static D2D1_MATRIX_3X2_F flux_translate_matrix(float x, float y) {
	D2D1_MATRIX_3X2_F m = flux_identity_matrix();
	m._31               = x;
	m._32               = y;
	return m;
}

static D2D1_MATRIX_3X2_F flux_matrix_multiply(D2D1_MATRIX_3X2_F const *a, D2D1_MATRIX_3X2_F const *b) {
	D2D1_MATRIX_3X2_F m;
	m._11 = a->_11 * b->_11 + a->_12 * b->_21;
	m._12 = a->_11 * b->_12 + a->_12 * b->_22;
	m._21 = a->_21 * b->_11 + a->_22 * b->_21;
	m._22 = a->_21 * b->_12 + a->_22 * b->_22;
	m._31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
	m._32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;
	return m;
}

static void
execute_clip_push(FluxRenderContext const *rc, FluxRenderCommand const *cmd, D2D1_MATRIX_3X2_F *stack, uint32_t *top) {
	D2D1_MATRIX_3X2_F current;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &current);
	if (*top < MAX_TRANSFORM_DEPTH) stack [(*top)++] = current;

	D2D1_RECT_F clip = {cmd->bounds.x, cmd->bounds.y, cmd->bounds.x + cmd->bounds.w, cmd->bounds.y + cmd->bounds.h};
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	D2D1_MATRIX_3X2_F translate = flux_translate_matrix(-cmd->scroll_x, -cmd->scroll_y);
	D2D1_MATRIX_3X2_F combined  = flux_matrix_multiply(&translate, &current);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
}

static void execute_clip_pop(FluxRenderContext const *rc, D2D1_MATRIX_3X2_F *stack, uint32_t *top) {
	D2D1_MATRIX_3X2_F restored = *top > 0 ? stack [--(*top)] : flux_identity_matrix();
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &restored);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void execute_draw(FluxEngine const *eng, FluxRenderContext const *rc, FluxRenderCommand const *cmd) {
	if (cmd->phase == FLUX_PHASE_MAIN) flux_dispatch_render(eng->store, rc, &cmd->snapshot, &cmd->bounds, &cmd->state);
	else flux_dispatch_render_overlay(eng->store, rc, &cmd->snapshot, &cmd->bounds);
}

void flux_engine_execute(FluxEngine const *eng, FluxRenderContext const *rc) {
	if (!eng || !rc) return;

	D2D1_MATRIX_3X2_F transform_stack [MAX_TRANSFORM_DEPTH];
	uint32_t          transform_top = 0;

	for (uint32_t i = 0; i < eng->commands.count; i++) {
		FluxRenderCommand const *cmd = &eng->commands.cmds [i];
		if (cmd->clip_action == FLUX_CLIP_PUSH) {
			execute_clip_push(rc, cmd, transform_stack, &transform_top);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_POP) {
			execute_clip_pop(rc, transform_stack, &transform_top);
			continue;
		}
		execute_draw(eng, rc, cmd);
	}
}
