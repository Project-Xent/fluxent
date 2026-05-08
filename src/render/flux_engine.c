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

typedef struct CollectContext {
	FluxEngine  *eng;
	XentContext *ctx;
} CollectContext;

static FluxRenderCommand collect_make_draw_command(
  FluxRenderSnapshot const *snapshot, FluxControlState state, XentRect const *rect, FluxCommandPhase phase
) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.snapshot    = *snapshot;
	cmd.bounds.x    = rect->x;
	cmd.bounds.y    = rect->y;
	cmd.bounds.w    = rect->width;
	cmd.bounds.h    = rect->height;
	cmd.state       = state;
	cmd.phase       = phase;
	cmd.clip_action = FLUX_CLIP_NONE;
	return cmd;
}

static void collect_emit_scroll_clip(FluxEngine *eng, FluxRenderSnapshot const *snapshot, XentRect const *rect) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.bounds.x    = rect->x;
	cmd.bounds.y    = rect->y;
	cmd.bounds.w    = rect->width;
	cmd.bounds.h    = rect->height;
	cmd.phase       = FLUX_PHASE_MAIN;
	cmd.clip_action = FLUX_CLIP_PUSH;
	cmd.scroll_x    = snapshot->scroll_x;
	cmd.scroll_y    = snapshot->scroll_y;
	flux_command_buffer_push(&eng->commands, &cmd);
}

static bool collect_effects(XentTraversalVisit const *visit, XentTraversalEffects *out_effects, void *userdata) {
	FluxEngine   *eng = ( FluxEngine * ) userdata;
	FluxNodeData *nd  = flux_node_store_get(eng->store, visit->node);
	if (!nd || !nd->component_data) return true;
	if (flux_node_store_control_type(eng->store, visit->node) != XENT_CONTROL_SCROLL) return true;
	FluxScrollData *scroll      = ( FluxScrollData * ) nd->component_data;
	out_effects->clips_children = true;
	out_effects->child_scroll_x = scroll->scroll_x;
	out_effects->child_scroll_y = scroll->scroll_y;
	return true;
}

static XentTraversalAction collect_enter(XentTraversalVisit const *visit, void *userdata) {
	CollectContext    *collect = ( CollectContext * ) userdata;
	FluxNodeData      *nd      = flux_node_store_get(collect->eng->store, visit->node);
	FluxRenderSnapshot snapshot;
	FluxControlState   state = flux_compute_control_state(nd);
	flux_snapshot_build(&snapshot, collect->ctx, visit->node, nd);

	FluxRenderCommand cmd = collect_make_draw_command(&snapshot, state, &visit->layout_rect, FLUX_PHASE_MAIN);
	flux_command_buffer_push(&collect->eng->commands, &cmd);
	if (visit->effects.clips_children) collect_emit_scroll_clip(collect->eng, &snapshot, &visit->layout_rect);
	return XENT_TRAVERSAL_CONTINUE;
}

static XentTraversalAction collect_leave(XentTraversalVisit const *visit, void *userdata) {
	CollectContext *collect = ( CollectContext * ) userdata;
	if (visit->effects.clips_children) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP;
		flux_command_buffer_push(&collect->eng->commands, &pop_cmd);
	}

	FluxNodeData      *nd = flux_node_store_get(collect->eng->store, visit->node);
	FluxRenderSnapshot snap;
	FluxControlState   state = flux_compute_control_state(nd);
	flux_snapshot_build(&snap, collect->ctx, visit->node, nd);
	FluxRenderCommand overlay_cmd = collect_make_draw_command(&snap, state, &visit->layout_rect, FLUX_PHASE_OVERLAY);
	flux_command_buffer_push(&collect->eng->commands, &overlay_cmd);
	return XENT_TRAVERSAL_CONTINUE;
}

static void collect_commands(FluxEngine *eng, XentContext *ctx, XentNodeId root) {
	CollectContext       collect = {eng, ctx};
	XentTraversalOptions options = {
	  .child_order      = XENT_CHILD_ORDER_FORWARD,
	  .cull_to_clip     = true,
	  .effects          = collect_effects,
	  .effects_userdata = eng,
	  .enter            = collect_enter,
	  .leave            = collect_leave,
	  .visit_userdata   = &collect,
	};
	xent_traverse_layout(ctx, root, &options);
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
