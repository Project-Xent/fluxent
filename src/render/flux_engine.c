#include "fluxent/flux_engine.h"
#include "fluxent/fluxent.h" /* flux_list_view_update_window */
#include "flux_fluent.h"
#include "flux_render_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct FluxCommandBuffer {
	FluxRenderCommand *cmds;
	uint32_t           count;
	uint32_t           capacity;
} FluxCommandBuffer;

struct FluxEngine {
	FluxNodeStore             *store;
	FluxControlRegistry const *registry;
	FluxCommandBuffer          commands;
	uint32_t                   transform_overflow_count;  /**< Bumped each time a clip/transform push is clamped. */
	uint32_t                   transform_overflow_logged; /**< Non-zero after first OutputDebugStringA notice. */
};

static bool flux_command_buffer_push(FluxCommandBuffer *buf, FluxRenderCommand const *cmd) {
	if (buf->count == buf->capacity) {
		uint32_t           new_cap  = buf->capacity ? buf->capacity * 2 : FLUX_RENDER_COMMAND_INITIAL_CAPACITY;
		FluxRenderCommand *new_cmds = ( FluxRenderCommand * ) realloc(buf->cmds, sizeof(FluxRenderCommand) * new_cap);
		if (!new_cmds) return false;
		buf->cmds     = new_cmds;
		buf->capacity = new_cap;
	}
	buf->cmds [buf->count++] = *cmd;
	return true;
}

static FluxControlState flux_compute_control_state(XentContext *ctx, XentNodeId node, FluxNodeData const *nd) {
	FluxControlState s = {0};
	s.enabled          = xent_get_semantic_enabled(ctx, node);
	if (!nd) return s;
	s.hovered      = nd->state.hovered;
	s.pressed      = nd->state.pressed;
	s.focused      = nd->state.focused;
	s.pointer_type = nd->state.pointer_type;
	return s;
}

typedef struct CollectFrame {
	XentNodeId         node;
	float              abs_x;
	float              abs_y;
	bool               main_emitted;
	bool               is_scroll;
	bool               clips_children;
	float              scroll_off_x;
	float              scroll_off_y;
	float              viewport_w;
	float              viewport_h;
	FluxRenderSnapshot snapshot;
	FluxControlState   state;
	XentNodeId         current_child;
	bool               has_transform; /**< Subtree wrapped in a render transform (scale/opacity). */
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
	cmd.bounds.w    = rect->w;
	cmd.bounds.h    = rect->h;
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
	cmd.bounds.w        = rect->w;
	cmd.bounds.h        = rect->h;
	cmd.phase           = FLUX_PHASE_MAIN;
	cmd.clip_action     = FLUX_CLIP_PUSH;
	cmd.scroll_x        = frame->snapshot.u.scroll.x;
	cmd.scroll_y        = frame->snapshot.u.scroll.y;
	frame->scroll_off_x = frame->snapshot.u.scroll.x;
	frame->scroll_off_y = frame->snapshot.u.scroll.y;
	frame->viewport_w   = rect->w;
	frame->viewport_h   = rect->h;
	flux_command_buffer_push(&eng->commands, &cmd);
}

/* Plain axis-aligned clip of a node's children to its rect (no scroll translate,
 * no viewport culling) — used to contain off-bounds children like NavView's
 * slid-away Minimal pane so it never bleeds outside the control. */
static void collect_emit_clip(FluxEngine *eng, CollectFrame const *frame, XentRect const *rect) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.phase       = FLUX_PHASE_MAIN;
	cmd.clip_action = FLUX_CLIP_PUSH;
	cmd.bounds      = (FluxRect) {frame->abs_x, frame->abs_y, rect->w, rect->h};
	flux_command_buffer_push(&eng->commands, &cmd);
}

static void
collect_emit_transform_push(FluxEngine *eng, CollectFrame const *frame, XentRect const *rect, FluxNodeData const *nd) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.phase        = FLUX_PHASE_MAIN;
	cmd.clip_action  = FLUX_CLIP_PUSH_TRANSFORM;
	cmd.scale        = nd->render_scale;
	cmd.opacity      = nd->render_opacity;
	cmd.translate_x  = nd->render_translate_x;
	cmd.translate_y  = nd->render_translate_y;
	cmd.pivot_x      = frame->abs_x + rect->w * 0.5f;
	cmd.pivot_y      = frame->abs_y + rect->h * 0.5f;
	cmd.clip_subtree = nd->render_clip_subtree;
	cmd.bounds       = (FluxRect) {frame->abs_x, frame->abs_y, rect->w, rect->h};
	flux_command_buffer_push(&eng->commands, &cmd);
}

/* Auto-derived scroll extent: the content size is the children's laid-out
 * bounding box plus the node's trailing padding, refreshed each frame before
 * the snapshot copies it, so pages never hand-compute their own height.
 * Owners that manage the extent themselves set FluxScrollData.content_manual. */
static void collect_update_scroll_extent(XentContext *ctx, XentNodeId node, FluxNodeData *nd, XentRect const *rect) {
	if (!nd || nd->component_type != FLUX_CONTROL_SCROLL || !nd->component_data) return;
	FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
	if (sd->content_manual) return;

	float max_r = 0.0f, max_b = 0.0f;
	for (XentNodeId child = xent_get_first_child(ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_get_next_sibling(ctx, child))
	{
		XentRect cr = {0};
		if (!xent_get_layout_rect(ctx, child, &cr)) continue;
		max_r = flux_maxf(max_r, cr.x + cr.w - rect->x);
		max_b = flux_maxf(max_b, cr.y + cr.h - rect->y);
	}

	XentResolvedInsets pad = {0};
	xent_get_resolved_padding(ctx, node, XENT_AXIS_VERTICAL, &pad);
	sd->content_w = max_r + pad.cross_end;
	sd->content_h = max_b + pad.main_end;
}

static void collect_emit_main(FluxEngine *eng, XentContext *ctx, CollectFrame *frame) {
	XentRect rect = {0};
	xent_get_layout_rect(ctx, frame->node, &rect);

	frame->abs_x     = rect.x;
	frame->abs_y     = rect.y;

	FluxNodeData *nd = ( FluxNodeData * ) xent_get_userdata(ctx, frame->node);
	collect_update_scroll_extent(ctx, frame->node, nd, &rect);
	/* Virtualized lists: fire the re-realize hook when scrolling (or a grid
	 * column-count change) has moved the viewport outside the realized cell
	 * window (same per-frame cadence as the scroll-extent refresh above). */
	if (nd && nd->component_data && xtk_is_list_type(( XtkControlType ) nd->component_type))
		flux_list_view_update_window(ctx, frame->node, nd);
	/* FlipView: size/lay pages to the viewport and apply the slide offset. */
	if (nd && nd->component_data && nd->component_type == FLUX_CONTROL_FLIP_VIEW)
		flux_flip_view_sync(ctx, frame->node, nd);
	/* BreadcrumbBar: re-run the overflow collapse against the assigned width. */
	if (nd && nd->component_data && nd->component_type == FLUX_CONTROL_BREADCRUMB_BAR)
		flux_breadcrumb_bar_sync(ctx, frame->node, nd);
	/* SplitView: position the content + pane wrappers for the current open state. */
	if (nd && nd->component_data && nd->component_type == FLUX_CONTROL_SPLIT_VIEW)
		flux_split_view_sync(ctx, frame->node, nd);
	flux_snapshot_build(&frame->snapshot, ctx, frame->node, nd);
	frame->state          = flux_compute_control_state(ctx, frame->node, nd);
	frame->is_scroll      = frame->snapshot.type == FLUX_CONTROL_SCROLL;
	frame->clips_children = nd && nd->clips_children;

	frame->has_transform
	  = nd
	 && (nd->render_scale != 1.0f || nd->render_opacity < 1.0f || nd->render_translate_y != 0.0f || nd->render_translate_x != 0.0f);
	if (frame->has_transform) collect_emit_transform_push(eng, frame, &rect, nd);

	FluxRenderCommand cmd = collect_make_draw_command(frame, &rect, FLUX_PHASE_MAIN);
	flux_command_buffer_push(&eng->commands, &cmd);
	if (frame->is_scroll) collect_emit_scroll_clip(eng, frame, &rect);
	else if (frame->clips_children) collect_emit_clip(eng, frame, &rect);

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

	if (child_rect.x + child_rect.w < vis_left) return false;
	if (child_rect.x > vis_right) return false;
	if (child_rect.y + child_rect.h < vis_top) return false;
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
	if (frame->is_scroll || frame->clips_children) {
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

	if (frame->has_transform) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP_TRANSFORM;
		flux_command_buffer_push(&eng->commands, &pop_cmd);
	}
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

FluxEngine *flux_engine_create(FluxNodeStore *store, FluxControlRegistry const *registry) {
	if (!store) return NULL;
	FluxEngine *eng = ( FluxEngine * ) calloc(1, sizeof(*eng));
	if (!eng) return NULL;
	eng->store    = store;
	eng->registry = registry;
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

typedef struct FluxTransformStack {
	D2D1_MATRIX_3X2_F frames [FLUX_RENDER_MAX_TRANSFORM_DEPTH];
	bool              clipped [FLUX_RENDER_MAX_TRANSFORM_DEPTH]; /**< Frame also pushed an axis-aligned clip. */
	uint32_t          top;
	uint32_t          clamped;                                   /**< Pushes coalesced into the topmost frame. */
} FluxTransformStack;

static void execute_note_overflow(FluxEngine *eng) {
	eng->transform_overflow_count++;
	if (!eng->transform_overflow_logged) {
		eng->transform_overflow_logged = 1;
		OutputDebugStringA("[fluxent] transform stack overflow: clip/scroll depth exceeded "
						   "FLUX_RENDER_MAX_TRANSFORM_DEPTH; nested frames coalesced.\n");
	}
	assert(0 && "Fluxent transform stack overflow (FLUX_RENDER_MAX_TRANSFORM_DEPTH).");
}

static void execute_clip_push(
  FluxEngine *eng, FluxRenderContext const *rc, FluxRenderCommand const *cmd, FluxTransformStack *stack
) {
	D2D1_MATRIX_3X2_F current;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &current);

	if (stack->top >= FLUX_RENDER_MAX_TRANSFORM_DEPTH) {
		stack->clamped++;
		execute_note_overflow(eng);
		D2D1_RECT_F clip = {cmd->bounds.x, cmd->bounds.y, cmd->bounds.x + cmd->bounds.w, cmd->bounds.y + cmd->bounds.h};
		ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		return;
	}

	stack->frames [stack->top++] = current;

	D2D1_RECT_F clip = {cmd->bounds.x, cmd->bounds.y, cmd->bounds.x + cmd->bounds.w, cmd->bounds.y + cmd->bounds.h};
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	D2D1_MATRIX_3X2_F translate = flux_translate_matrix(-cmd->scroll_x, -cmd->scroll_y);
	D2D1_MATRIX_3X2_F combined  = flux_matrix_multiply(&translate, &current);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
}

static void execute_clip_pop(FluxRenderContext const *rc, FluxTransformStack *stack) {
	if (stack->clamped > 0) {
		stack->clamped--;
		ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
		return;
	}

	D2D1_MATRIX_3X2_F restored = stack->top > 0 ? stack->frames [--stack->top] : flux_identity_matrix();
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &restored);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void execute_transform_push(
  FluxEngine *eng, FluxRenderContext const *rc, FluxRenderCommand const *cmd, FluxTransformStack *stack
) {
	D2D1_LAYER_PARAMETERS lp;
	lp.contentBounds     = (D2D1_RECT_F) {-1.0e9f, -1.0e9f, 1.0e9f, 1.0e9f};
	lp.geometricMask     = NULL;
	lp.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	lp.maskTransform     = flux_identity_matrix();
	lp.opacity           = cmd->opacity;
	lp.opacityBrush      = NULL;
	lp.layerOptions      = D2D1_LAYER_OPTIONS_NONE;
	ID2D1RenderTarget_PushLayer(FLUX_RT(rc), &lp, NULL);

	D2D1_MATRIX_3X2_F current;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &current);
	if (stack->top >= FLUX_RENDER_MAX_TRANSFORM_DEPTH) {
		stack->clamped++;
		execute_note_overflow(eng);
		return;
	}
	stack->frames [stack->top]  = current;

	/* Optional subtree clip, recorded in the parent (pre-transform) space so a
	 * slid child is clipped to its own layout rect — content sliding out from
	 * behind the Expander header never overdraws the header. */
	stack->clipped [stack->top] = cmd->clip_subtree;
	if (cmd->clip_subtree) {
		D2D1_RECT_F clip = {cmd->bounds.x, cmd->bounds.y, cmd->bounds.x + cmd->bounds.w, cmd->bounds.y + cmd->bounds.h};
		ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	}
	stack->top++;

	float             s = cmd->scale;
	D2D1_MATRIX_3X2_F scale;
	scale._11                  = s;
	scale._12                  = 0.0f;
	scale._21                  = 0.0f;
	scale._22                  = s;
	scale._31                  = cmd->pivot_x * (1.0f - s) + cmd->translate_x;
	scale._32                  = cmd->pivot_y * (1.0f - s) + cmd->translate_y;
	D2D1_MATRIX_3X2_F combined = flux_matrix_multiply(&scale, &current);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
}

static void transform_restore_top(FluxRenderContext const *rc, FluxTransformStack *stack) {
	D2D1_MATRIX_3X2_F restored = stack->frames [--stack->top];
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &restored);
	if (stack->clipped [stack->top]) ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void execute_transform_pop(FluxRenderContext const *rc, FluxTransformStack *stack) {
	if (stack->clamped > 0) stack->clamped--;
	else if (stack->top > 0) transform_restore_top(rc, stack);
	ID2D1RenderTarget_PopLayer(FLUX_RT(rc));
}

static void execute_draw(FluxEngine const *eng, FluxRenderContext const *rc, FluxRenderCommand const *cmd) {
	if (cmd->phase == FLUX_PHASE_MAIN)
		flux_engine_dispatch_render(eng->registry, rc, &cmd->snapshot, &cmd->bounds, &cmd->state);
	else flux_engine_dispatch_render_overlay(eng->registry, rc, &cmd->snapshot, &cmd->bounds);
}

void flux_engine_execute(FluxEngine const *eng, FluxRenderContext const *rc) {
	if (!eng || !rc) return;

	FluxTransformStack stack = {0};
	FluxEngine        *mut   = ( FluxEngine * ) eng;

	for (uint32_t i = 0; i < eng->commands.count; i++) {
		FluxRenderCommand const *cmd = &eng->commands.cmds [i];
		if (cmd->clip_action == FLUX_CLIP_PUSH) {
			execute_clip_push(mut, rc, cmd, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_POP) {
			execute_clip_pop(rc, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_PUSH_TRANSFORM) {
			execute_transform_push(mut, rc, cmd, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_POP_TRANSFORM) {
			execute_transform_pop(rc, &stack);
			continue;
		}
		execute_draw(eng, rc, cmd);
	}
}
