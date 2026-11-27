#include "store/flux_node_data.h"
#include "render/flux_dl_replay.h"
#include "flux_display_paint.h"
#include "flux_fluent.h"
#include "controls/flux_module.h"
#include "controls/factory/flux_init_container.h"
#include "flux_render_internal.h"
#include "render/flux_lift.h"
#include "render/flux_projection.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <xtk/xtk_alloc.h>
#include <xtk/xtk.h>

typedef struct FluxCommandBuffer {
	FluxRenderCommand *cmds;
	uint32_t           count;
	uint32_t           capacity;
} FluxCommandBuffer;

struct FluxEngine {
	FluxStore        *store;
	XtkBackend       *backend;
	FluxCommandBuffer commands;
	XentNodeId        root;
	uint32_t          transform_overflow_count;
	uint32_t          transform_overflow_logged;
	bool              render_degraded;
	FluxDegradedFn    degraded_cb;
	void             *degraded_ctx;
	bool              has_dirty;
	bool              force_full;
	bool              has_env;
	uint64_t          env_hash;
	FluxRect          dirty_rect;
	FluxRect          prev_dirty;
};

/* Float fields on the node can be rewritten with the same computed value; treat that as
   unchanged so a static node is not repainted every frame. */
static bool render_close(float a, float b) { return fabsf(a - b) <= 0.001f; }

static void engine_expand_dirty(FluxEngine *eng, FluxRect const *r, float pad) {
	if (!eng || !r || r->w <= 0.0f || r->h <= 0.0f) return;
	FluxRect d = {r->x - pad, r->y - pad, r->w + pad * 2.0f, r->h + pad * 2.0f};
	if (!eng->has_dirty) {
		eng->dirty_rect = d;
		eng->has_dirty  = true;
		return;
	}
	float x0        = fminf(eng->dirty_rect.x, d.x);
	float y0        = fminf(eng->dirty_rect.y, d.y);
	float x1        = fmaxf(eng->dirty_rect.x + eng->dirty_rect.w, d.x + d.w);
	float y1        = fmaxf(eng->dirty_rect.y + eng->dirty_rect.h, d.y + d.h);
	eng->dirty_rect = (FluxRect) {x0, y0, x1 - x0, y1 - y0};
}

void flux_engine_note_removed(FluxEngine *eng, XentNodeId id) {
	if (!eng || !eng->store || id == XENT_NODE_INVALID) return;
	FluxNodeData const *nd = flux_store_find(eng->store, id);
	if (nd && nd->has_prev_bounds) engine_expand_dirty(eng, &nd->prev_bounds, 4.0f);
}

void flux_engine_invalidate_full(FluxEngine *eng) {
	if (eng) eng->force_full = true;
}

static bool command_buffer_push(FluxCommandBuffer *buf, FluxRenderCommand const *cmd) {
	if (buf->count == buf->capacity) {
		uint32_t           new_cap = buf->capacity ? buf->capacity * 2 : FLUX_RENDER_COMMAND_INITIAL_CAPACITY;
		FluxRenderCommand *new_cmds
		  = ( FluxRenderCommand * ) xtk_alloc_resize(buf->cmds, sizeof(FluxRenderCommand) * new_cap);
		if (!new_cmds) return false;
		buf->cmds     = new_cmds;
		buf->capacity = new_cap;
	}
	buf->cmds [buf->count++] = *cmd;
	return true;
}

static void engine_push(FluxEngine *eng, FluxRenderCommand const *cmd) {
	if (!command_buffer_push(&eng->commands, cmd)) eng->render_degraded = true;
}

static FluxControlState compute_control_state(XentCtx *ctx, XentNodeId node, FluxNodeData const *nd) {
	FluxControlState s = {0};
	s.enabled          = xent_sem_enabled(ctx, node);
	if (!nd) return s;
	s.hovered      = nd->state.hovered;
	s.pressed      = nd->state.pressed;
	s.focused      = nd->state.focused && !nd->state.suppress_focus;
	s.pointer_type = nd->state.pointer_type;
	return s;
}

typedef struct CollectFrame {
	XentNodeId        node;
	float             abs_x;
	float             abs_y;
	bool              main_emitted;
	bool              is_scroll;
	bool              clips_children;
	float             scroll_off_x;
	float             scroll_off_y;
	float             cum_scroll_x;
	float             cum_scroll_y;
	float             viewport_w;
	float             viewport_h;
	FluxRenderSnap    snapshot;
	FluxControlState  state;
	XentNodeId        current_child;
	bool              has_transform;
	bool              clipped_top;
	bool              dim_item_content;
	FluxItemTextColor item_text_color;
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
	cmd.scroll_x        = frame->snapshot.u.scroll.x - frame->snapshot.u.scroll.origin_x;
	cmd.scroll_y        = frame->snapshot.u.scroll.y - frame->snapshot.u.scroll.origin_y;
	frame->scroll_off_x = cmd.scroll_x;
	frame->scroll_off_y = cmd.scroll_y;
	frame->viewport_w   = rect->w;
	frame->viewport_h   = rect->h;
	engine_push(eng, &cmd);
}

static void collect_emit_clip(FluxEngine *eng, CollectFrame const *frame, XentRect const *rect) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.phase       = FLUX_PHASE_MAIN;
	cmd.clip_action = FLUX_CLIP_PUSH;
	cmd.bounds      = (FluxRect) {frame->abs_x, frame->abs_y, rect->w, rect->h};
	engine_push(eng, &cmd);
}

static void collect_emit_clip_top(FluxEngine *eng, CollectFrame const *frame, XentRect const *rect, float top) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.phase       = FLUX_PHASE_MAIN;
	cmd.clip_action = FLUX_CLIP_PUSH;
	cmd.bounds      = (FluxRect) {frame->abs_x, frame->abs_y + top, rect->w, rect->h > top ? rect->h - top : 0.0f};
	engine_push(eng, &cmd);
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
	cmd.rotate_x     = nd->render_rotate_x;
	cmd.rotate_y     = nd->render_rotate_y;
	cmd.pivot_x      = frame->abs_x + rect->w * 0.5f;
	cmd.pivot_y      = frame->abs_y + rect->h * 0.5f;
	cmd.clip_subtree = nd->render_clip_subtree;
	cmd.bounds       = (FluxRect) {frame->abs_x, frame->abs_y, rect->w, rect->h};
	engine_push(eng, &cmd);
}

static void collect_update_scroll_extent(FluxEngine *eng, XentCtx *ctx, XentNodeId node, FluxNodeData *nd) {
	if (!nd || nd->component_type != FLUX_CONTROL_SCROLL) return;
	FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
	float           cw = sd ? sd->content_w : 0.0f;
	float           ch = sd ? sd->content_h : 0.0f;
	flux_scroll_measure(ctx, node, sd);
	if (sd && (sd->content_w != cw || sd->content_h != ch))
		flux_uia_mark_dirty(eng->store, node, FLUX_UIA_DIRTY_PATTERN);
}

static FluxItemTextColor collect_item_text_color(FluxRenderSnap const *snap, FluxControlState const *state) {
	uint8_t kind = snap->u.list_item.kind;
	if (kind == XTK_LIST_KIND_GRID) return FLUX_ITEM_TEXT_WINDOW;
	if (kind != XTK_LIST_KIND_LIST && kind != XTK_LIST_KIND_LIST_BOX) return FLUX_ITEM_TEXT_INHERIT;
	if (kind == XTK_LIST_KIND_LIST_BOX && !state->enabled) return FLUX_ITEM_TEXT_DISABLED;
	if (snap->u.list_item.is_selected || (state->enabled && (state->hovered || state->pressed)))
		return FLUX_ITEM_TEXT_HIGHLIGHT;
	return FLUX_ITEM_TEXT_BUTTON;
}

static void collect_emit_item_opacity(FluxEngine *eng, CollectFrame const *frame, bool push) {
	FluxRenderCommand cmd = {
	  .phase       = FLUX_PHASE_MAIN,
	  .clip_action = push ? FLUX_CLIP_PUSH_TRANSFORM : FLUX_CLIP_POP_TRANSFORM,
	  .scale       = 1.0f,
	  .opacity     = 0.3f,
	  .pivot_x     = frame->abs_x,
	  .pivot_y     = frame->abs_y,
	};
	engine_push(eng, &cmd);
}

static void collect_item_content(CollectFrame *frame) {
	if (frame->snapshot.type == FLUX_CONTROL_TEXT) frame->snapshot.u.text.item_color = frame->item_text_color;
	if (frame->snapshot.type != FLUX_CONTROL_LIST_ITEM) return;
	frame->item_text_color  = collect_item_text_color(&frame->snapshot, &frame->state);
	uint8_t kind            = frame->snapshot.u.list_item.kind;
	frame->dim_item_content = !frame->state.enabled && (kind == XTK_LIST_KIND_LIST || kind == XTK_LIST_KIND_GRID);
}

static void collect_control_snapshot(FluxEngine *eng, XentCtx *ctx, CollectFrame *frame, FluxNodeData *nd) {
	collect_update_scroll_extent(eng, ctx, frame->node, nd);
	FluxControlModule const *m = nd && nd->component_data ? flux_module_get(nd->component_type) : NULL;
	if (m && m->tree && m->tree->collect) ( void ) m->tree->collect(ctx, eng->store, frame->node, nd);
	flux_snapshot_build(&frame->snapshot, ctx, frame->node, nd);
	frame->state = compute_control_state(ctx, frame->node, nd);
	collect_item_content(frame);
}

static void collect_track_node_dirty(
  FluxEngine *eng, CollectFrame const *frame, XentRect const *rect, FluxNodeData *nd, bool has_transform
) {
	if (!nd) {
		eng->force_full = true;
		return;
	}
	FluxRect bounds = {frame->abs_x - frame->cum_scroll_x, frame->abs_y - frame->cum_scroll_y, rect->w, rect->h};
	bool     dyn    = false;
	uint64_t chash  = flux_snap_hash(&frame->snapshot, &frame->state, rect->w, rect->h, NULL, &dyn);
	bool     moved  = !nd->has_prev_bounds || memcmp(&nd->prev_bounds, &bounds, sizeof(bounds)) != 0;
	/*
	 * Render transforms live on FluxNodeData and never reach the snapshot, so a hash
	 * comparison alone cannot see an animated scale, opacity or translate. Without
	 * this, a dialog that fades in parks on its first (nearly transparent) frame:
	 * nothing else changes, so the frame produces an empty dirty rect and the
	 * present is skipped. Compare only against a recorded previous frame, so the
	 * first sighting of a node does not force a full repaint. The epsilon keeps a
	 * node that rewrites the same computed value from forcing a full repaint on
	 * float noise.
	 */
	bool transform_moved = nd->has_prev_render
	                    && (!render_close(nd->prev_render_scale, nd->render_scale)
	                        || !render_close(nd->prev_render_opacity, nd->render_opacity)
	                        || !render_close(nd->prev_render_translate_x, nd->render_translate_x)
	                        || !render_close(nd->prev_render_translate_y, nd->render_translate_y)
	                        || !render_close(nd->prev_render_rotate_x, nd->render_rotate_x)
	                        || !render_close(nd->prev_render_rotate_y, nd->render_rotate_y)
	                        || nd->prev_render_clip_subtree != nd->render_clip_subtree);
	bool changed = moved || nd->collect_hash != chash || dyn || nd->dl_dynamic || transform_moved;
	/* A transform moves its whole subtree and invalidates the previous frame's pixels
	   under the old projection, so the affected area cannot be described by this node's
	   bounds alone. Repaint the frame while a transform changes. */
	if (changed && (has_transform || transform_moved)) eng->force_full = true;
	if (changed) {
		if (nd->has_prev_bounds) engine_expand_dirty(eng, &nd->prev_bounds, 4.0f);
		engine_expand_dirty(eng, &bounds, 4.0f);
		/*
		 * Advance the transform baseline only on a frame that was actually dirtied. If it
		 * moved on every collect, an animation that steps by less than the epsilon each
		 * frame would compare as unchanged forever: the baseline would creep along with
		 * the value and no single step could ever cross the threshold.
		 */
		nd->has_prev_render          = true;
		nd->prev_render_scale        = nd->render_scale;
		nd->prev_render_opacity      = nd->render_opacity;
		nd->prev_render_translate_x  = nd->render_translate_x;
		nd->prev_render_translate_y  = nd->render_translate_y;
		nd->prev_render_rotate_x     = nd->render_rotate_x;
		nd->prev_render_rotate_y     = nd->render_rotate_y;
		nd->prev_render_clip_subtree = nd->render_clip_subtree;
	}
	nd->prev_bounds     = bounds;
	nd->has_prev_bounds = true;
	nd->collect_hash    = chash;
	if (frame->is_scroll && nd->component_data) {
		float sx = frame->scroll_off_x, sy = frame->scroll_off_y;
		if (sx != nd->prev_scroll_x || sy != nd->prev_scroll_y) {
			engine_expand_dirty(eng, &bounds, 4.0f);
			flux_uia_mark_dirty(eng->store, frame->node, FLUX_UIA_DIRTY_PATTERN);
			nd->prev_scroll_x = sx;
			nd->prev_scroll_y = sy;
		}
	}
}

static void collect_emit_main(FluxEngine *eng, XentCtx *ctx, CollectFrame *frame) {
	XentRect rect = {0};
	xent_layout_rect(ctx, frame->node, &rect);

	frame->abs_x     = rect.x;
	frame->abs_y     = rect.y;

	FluxNodeData *nd = flux_store_find(eng->store, frame->node);
	collect_control_snapshot(eng, ctx, frame, nd);
	frame->is_scroll      = frame->snapshot.type == FLUX_CONTROL_SCROLL;
	frame->clips_children = nd && nd->clips_children;

	frame->has_transform
	  = nd
	 && (nd->render_scale != 1.0f || nd->render_opacity < 1.0f || nd->render_translate_y != 0.0f || nd->render_translate_x != 0.0f || nd->render_rotate_x != 0.0f || nd->render_rotate_y != 0.0f);
	if (frame->has_transform) collect_emit_transform_push(eng, frame, &rect, nd);
	frame->clipped_top = nd && nd->clip_top > 0.0f;
	if (frame->clipped_top) collect_emit_clip_top(eng, frame, &rect, nd->clip_top);

	FluxRenderCommand cmd = collect_make_draw_command(frame, &rect, FLUX_PHASE_MAIN);
	engine_push(eng, &cmd);
	if (frame->dim_item_content) collect_emit_item_opacity(eng, frame, true);
	if (frame->is_scroll) collect_emit_scroll_clip(eng, frame, &rect);
	else if (frame->clips_children) collect_emit_clip(eng, frame, &rect);
	collect_track_node_dirty(eng, frame, &rect, nd, frame->has_transform);

	frame->current_child = xent_node_first(ctx, frame->node);
	frame->main_emitted  = true;
}

/*
 * A subtree that becomes collapsed is skipped by the walk, so its previous pixels are
 * never marked dirty and the present leaves them on screen: closing a dialog showed a
 * stale blank rectangle where the buttons had been. Every node in the subtree kept its
 * own prev_bounds while it was visible, so the whole subtree has to be visited.
 *
 * Iterative on purpose: this runs for every collapsed child of every frame, and a deep
 * subtree would otherwise recurse to the depth of the tree.
 *
 * prev_bounds is the pre-transform layout rect, so a node that was scaled or translated
 * could have painted outside it. Ask for a full repaint in that case rather than guess
 * the painted area.
 */
static void collect_note_hidden(FluxEngine *eng, XentCtx *ctx, XentNodeId root) {
	uint32_t    cap   = 64;
	uint32_t    top   = 0;
	XentNodeId *stack = ( XentNodeId * ) xtk_alloc(sizeof(*stack) * cap);
	if (!stack) {
		eng->force_full = true;
		return;
	}
	stack [top++] = root;

	while (top > 0) {
		XentNodeId    node = stack [--top];
		FluxNodeData *nd   = flux_store_find(eng->store, node);
		if (nd) {
			if (nd->has_prev_bounds) engine_expand_dirty(eng, &nd->prev_bounds, 4.0f);
			/* The recorded bounds are not the painted bounds once a transform applies. */
			if (nd->render_scale != 1.0f
			    || nd->render_translate_x != 0.0f
			    || nd->render_translate_y != 0.0f
			    || nd->render_rotate_x != 0.0f
			    || nd->render_rotate_y != 0.0f)
				eng->force_full = true;
			nd->has_prev_bounds = false;
			nd->has_prev_render = false;
			nd->collect_hash    = 0u;
		}

		for (XentNodeId child = xent_node_first(ctx, node); child != XENT_NODE_INVALID;
		  child               = xent_node_next(ctx, child))
		{
			if (top == cap) {
				uint32_t    next_cap = cap * 2u;
				XentNodeId *grown    = ( XentNodeId * ) xtk_alloc_resize(stack, sizeof(*grown) * next_cap);
				if (!grown) {
					xtk_alloc_free(stack);
					eng->force_full = true;
					return;
				}
				stack = grown;
				cap   = next_cap;
			}
			stack [top++] = child;
		}
	}

	xtk_alloc_free(stack);
}

static bool collect_child_visible(XentCtx *ctx, CollectFrame const *frame, XentNodeId child) {
	if (!frame->is_scroll) return true;

	XentRect child_rect = {0};
	xent_layout_rect(ctx, child, &child_rect);

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
	CollectFrame *new_stack = ( CollectFrame * ) xtk_alloc_resize(*stack, sizeof(CollectFrame) * new_cap);
	if (!new_stack) return false;
	*stack     = new_stack;
	*stack_cap = new_cap;
	return true;
}

static bool collect_push_child(
  CollectFrame **stack, uint32_t *stack_top, uint32_t *stack_cap, XentNodeId child, FluxItemTextColor item_color,
  float cum_scroll_x, float cum_scroll_y
) {
	if (*stack_top == *stack_cap && !collect_grow_stack(stack, stack_cap)) return false;
	CollectFrame frame        = collect_root_frame(child);
	frame.item_text_color     = item_color;
	frame.cum_scroll_x        = cum_scroll_x;
	frame.cum_scroll_y        = cum_scroll_y;
	(*stack) [(*stack_top)++] = frame;
	return true;
}

static bool
collect_visit_child(FluxEngine *eng, CollectFrame **stack, uint32_t *stack_top, uint32_t *stack_cap, XentCtx *ctx) {
	CollectFrame *frame = &(*stack) [*stack_top - 1];
	if (frame->current_child == XENT_NODE_INVALID) return false;

	XentNodeId child     = frame->current_child;
	frame->current_child = xent_node_next(ctx, child);
	if (xent_collapsed(ctx, child)) {
		collect_note_hidden(eng, ctx, child);
		return true;
	}
	if (frame->node != eng->root && flux_node_lifted(eng->store, child)) return true;
	if (!collect_child_visible(ctx, frame, child)) return true;

	float sx = frame->cum_scroll_x + (frame->is_scroll ? frame->scroll_off_x : 0.0f);
	float sy = frame->cum_scroll_y + (frame->is_scroll ? frame->scroll_off_y : 0.0f);
	if (!collect_push_child(stack, stack_top, stack_cap, child, frame->item_text_color, sx, sy))
		eng->render_degraded = true;
	return true;
}

static void collect_emit_finish(FluxEngine *eng, XentCtx *ctx, CollectFrame const *frame) {
	if (frame->is_scroll || frame->clips_children) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP;
		engine_push(eng, &pop_cmd);
	}
	if (frame->dim_item_content) collect_emit_item_opacity(eng, frame, false);

	XentRect rect = {0};
	xent_layout_rect(ctx, frame->node, &rect);
	FluxRenderCommand overlay_cmd = collect_make_draw_command(frame, &rect, FLUX_PHASE_OVERLAY);
	engine_push(eng, &overlay_cmd);

	if (frame->clipped_top) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP;
		engine_push(eng, &pop_cmd);
	}

	if (frame->has_transform) {
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP_TRANSFORM;
		engine_push(eng, &pop_cmd);
	}
}

static void collect_lifted(FluxEngine *eng, XentCtx *ctx, XentNodeId layer);

static void collect_commands_at(FluxEngine *eng, XentCtx *ctx, XentNodeId root, float scroll_x, float scroll_y) {
	uint32_t      stack_cap = 64;
	uint32_t      stack_top = 0;
	CollectFrame *stack     = ( CollectFrame * ) xtk_alloc(sizeof(CollectFrame) * stack_cap);
	if (!stack) {
		eng->render_degraded = true;
		return;
	}

	CollectFrame root_frame = collect_root_frame(root);
	root_frame.cum_scroll_x = scroll_x;
	root_frame.cum_scroll_y = scroll_y;
	stack [stack_top++]     = root_frame;
	while (stack_top > 0) {
		CollectFrame *frame = &stack [stack_top - 1];
		if (!frame->main_emitted) {
			collect_emit_main(eng, ctx, frame);
			continue;
		}
		if (collect_visit_child(eng, &stack, &stack_top, &stack_cap, ctx)) continue;
		collect_emit_finish(eng, ctx, frame);
		XentNodeId done = frame->node;
		stack_top--;
		if (xent_node_parent(ctx, done) == eng->root) collect_lifted(eng, ctx, done);
	}

	xtk_alloc_free(stack);
}

static void collect_commands(FluxEngine *eng, XentCtx *ctx, XentNodeId root) {
	collect_commands_at(eng, ctx, root, 0.0f, 0.0f);
}

static void collect_lift_push(FluxEngine *eng, FluxLiftPlace const *place) {
	FluxRenderCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.phase       = FLUX_PHASE_MAIN;
	cmd.clip_action = FLUX_CLIP_PUSH_TRANSFORM;
	cmd.scale       = 1.0f;
	cmd.opacity     = 1.0f;
	cmd.translate_x = place->dx;
	cmd.translate_y = place->dy;
	engine_push(eng, &cmd);
}

static void collect_lifted(FluxEngine *eng, XentCtx *ctx, XentNodeId layer) {
	for (uint32_t i = 0; i < flux_store_nlifted(eng->store); i++) {
		XentNodeId    node  = flux_store_lifted(eng->store, i);
		FluxLiftPlace place = {0};
		if (!flux_lift_place(ctx, eng->store, node, &place) || place.layer != layer) continue;
		collect_lift_push(eng, &place);
		collect_commands_at(eng, ctx, node, -place.dx, -place.dy);
		FluxRenderCommand pop_cmd;
		memset(&pop_cmd, 0, sizeof(pop_cmd));
		pop_cmd.phase       = FLUX_PHASE_MAIN;
		pop_cmd.clip_action = FLUX_CLIP_POP_TRANSFORM;
		engine_push(eng, &pop_cmd);
	}
}

FluxEngine *flux_engine_create(FluxStore *store) {
	if (!store) return NULL;
	FluxEngine *eng = ( FluxEngine * ) calloc(1, sizeof(*eng));
	if (!eng) return NULL;
	eng->store = store;
	return eng;
}

void flux_engine_bind(FluxEngine *eng, XtkBackend *backend) {
	if (!eng) return;
	eng->backend = backend;
}

void flux_engine_ondegraded(FluxEngine *eng, FluxDegradedFn cb, void *ctx) {
	if (!eng) return;
	eng->degraded_cb  = cb;
	eng->degraded_ctx = ctx;
}

void flux_engine_destroy(FluxEngine *eng) {
	if (!eng) return;
	xtk_alloc_free(eng->commands.cmds);
	free(eng);
}

static void collect_report_degraded(FluxEngine *eng) {
	OutputDebugStringA("[fluxent] render degraded: collect allocation failed; part of the frame was dropped.\n");
	if (eng->degraded_cb) {
		eng->degraded_cb(eng->degraded_ctx);
		return;
	}
	assert(0 && "Fluxent render degraded (collect allocation failure).");
}

void flux_engine_collect(FluxEngine *eng, XentCtx *ctx, XentNodeId root) {
	if (!eng || !ctx || root == XENT_NODE_INVALID) return;
	eng->commands.count  = 0;
	eng->render_degraded = false;
	eng->root            = root;
	collect_commands(eng, ctx, root);
	if (flux_store_nlifted(eng->store) > 0u) eng->force_full = true;
	if (eng->render_degraded) collect_report_degraded(eng);
}

static FluxRect rect_union_nonempty(FluxRect const *a, FluxRect const *b) {
	if (a->w <= 0.0f || a->h <= 0.0f) return *b;
	if (b->w <= 0.0f || b->h <= 0.0f) return *a;
	float x0 = fminf(a->x, b->x), y0 = fminf(a->y, b->y);
	float x1 = fmaxf(a->x + a->w, b->x + b->w), y1 = fmaxf(a->y + a->h, b->y + b->h);
	return (FluxRect) {x0, y0, x1 - x0, y1 - y0};
}

static FluxRect rect_clamp_viewport(FluxRect r, float max_w, float max_h) {
	if (r.w <= 0.0f || r.h <= 0.0f) return (FluxRect) {0};
	float x0 = r.x < 0.0f ? 0.0f : r.x;
	float y0 = r.y < 0.0f ? 0.0f : r.y;
	float x1 = (r.x + r.w) > max_w ? max_w : (r.x + r.w);
	float y1 = (r.y + r.h) > max_h ? max_h : (r.y + r.h);
	return (x1 > x0 && y1 > y0) ? (FluxRect) {x0, y0, x1 - x0, y1 - y0} : (FluxRect) {0};
}

bool flux_engine_dirty_rect(
  FluxEngine const *eng, uint64_t env_hash, float max_w, float max_h, bool double_buffered, FluxRect *out_draw
) {
	if (!eng || !out_draw || !eng->has_env || eng->env_hash != env_hash || eng->force_full) return false;
	FluxRect cur  = eng->has_dirty ? eng->dirty_rect : (FluxRect) {0};
	FluxRect comb = double_buffered ? rect_union_nonempty(&cur, &eng->prev_dirty) : cur;
	*out_draw     = rect_clamp_viewport(comb, max_w, max_h);
	return true;
}

void flux_engine_finish_frame(FluxEngine *eng, uint64_t env_hash) {
	if (!eng) return;
	eng->env_hash   = env_hash;
	eng->has_env    = true;
	eng->prev_dirty = eng->has_dirty ? eng->dirty_rect : (FluxRect) {0};
	eng->has_dirty  = false;
	eng->force_full = false;
	eng->dirty_rect = (FluxRect) {0};
}

uint32_t                 flux_engine_ncmd(FluxEngine const *eng) { return eng ? eng->commands.count : 0; }

FluxRenderCommand const *flux_engine_cmd(FluxEngine const *eng, uint32_t index) {
	if (!eng || index >= eng->commands.count) return NULL;
	return &eng->commands.cmds [index];
}

static D2D1_MATRIX_3X2_F identity_matrix(void) {
	D2D1_MATRIX_3X2_F m;
	m._11 = 1.0f;
	m._12 = 0.0f;
	m._21 = 0.0f;
	m._22 = 1.0f;
	m._31 = 0.0f;
	m._32 = 0.0f;
	return m;
}

static D2D1_MATRIX_3X2_F translate_matrix(float x, float y) {
	D2D1_MATRIX_3X2_F m = identity_matrix();
	m._31               = x;
	m._32               = y;
	return m;
}

static D2D1_MATRIX_3X2_F matrix_multiply(D2D1_MATRIX_3X2_F const *a, D2D1_MATRIX_3X2_F const *b) {
	D2D1_MATRIX_3X2_F m;
	m._11 = a->_11 * b->_11 + a->_12 * b->_21;
	m._12 = a->_11 * b->_12 + a->_12 * b->_22;
	m._21 = a->_21 * b->_11 + a->_22 * b->_21;
	m._22 = a->_21 * b->_12 + a->_22 * b->_22;
	m._31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
	m._32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;
	return m;
}

#define FLUX_RENDER_MAX_PROJECTIONS 4

typedef struct FluxTransformStack {
	D2D1_MATRIX_3X2_F frames [FLUX_RENDER_MAX_TRANSFORM_DEPTH];
	bool              clipped [FLUX_RENDER_MAX_TRANSFORM_DEPTH];
	bool              projected [FLUX_RENDER_MAX_TRANSFORM_DEPTH];
	uint32_t          top;
	uint32_t          clamped;
	FluxProjection    proj [FLUX_RENDER_MAX_PROJECTIONS];
	uint32_t          nproj;
} FluxTransformStack;

static FluxRenderCtx const *stack_rc(FluxTransformStack const *stack, FluxRenderCtx const *root) {
	return stack->nproj ? &stack->proj [stack->nproj - 1].rc : root;
}

static void execute_note_overflow(FluxEngine *eng) {
	eng->transform_overflow_count++;
	if (!eng->transform_overflow_logged) {
		eng->transform_overflow_logged = 1;
		OutputDebugStringA(
		  "[fluxent] transform stack overflow: clip/scroll depth exceeded "
		  "FLUX_RENDER_MAX_TRANSFORM_DEPTH; nested frames coalesced.\n"
		);
	}
	assert(0 && "Fluxent transform stack overflow (FLUX_RENDER_MAX_TRANSFORM_DEPTH).");
}

static void
execute_clip_push(FluxEngine *eng, FluxRenderCtx const *rc, FluxRenderCommand const *cmd, FluxTransformStack *stack) {
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

	D2D1_MATRIX_3X2_F translate = translate_matrix(-cmd->scroll_x, -cmd->scroll_y);
	D2D1_MATRIX_3X2_F combined  = matrix_multiply(&translate, &current);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
}

static void execute_clip_pop(FluxRenderCtx const *rc, FluxTransformStack *stack) {
	if (stack->clamped > 0) {
		stack->clamped--;
		ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
		return;
	}

	D2D1_MATRIX_3X2_F restored = stack->top > 0 ? stack->frames [--stack->top] : identity_matrix();
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &restored);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static bool transform_project(
  FluxRenderCtx const *rc, FluxRenderCommand const *cmd, FluxTransformStack *stack, D2D1_MATRIX_3X2_F const *content
) {
	if ((cmd->rotate_x == 0.0f && cmd->rotate_y == 0.0f) || stack->nproj == FLUX_RENDER_MAX_PROJECTIONS) return false;
	float cx = cmd->pivot_x * content->_11 + cmd->pivot_y * content->_21 + content->_31;
	float cy = cmd->pivot_x * content->_12 + cmd->pivot_y * content->_22 + content->_32;
	if (!flux_projection_begin(&stack->proj [stack->nproj], rc, content, cx, cy, cmd->rotate_x, cmd->rotate_y))
		return false;
	stack->nproj++;
	stack->projected [stack->top - 1] = true;
	return true;
}

static void execute_transform_push(
  FluxEngine *eng, FluxRenderCtx const *rc, FluxRenderCommand const *cmd, FluxTransformStack *stack
) {
	D2D1_LAYER_PARAMETERS lp;
	lp.contentBounds     = (D2D1_RECT_F) {-1.0e9f, -1.0e9f, 1.0e9f, 1.0e9f};
	lp.geometricMask     = NULL;
	lp.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	lp.maskTransform     = identity_matrix();
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
	stack->frames [stack->top]    = current;
	stack->projected [stack->top] = false;
	stack->clipped [stack->top]   = cmd->clip_subtree;
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
	D2D1_MATRIX_3X2_F combined = matrix_multiply(&scale, &current);
	if (!transform_project(rc, cmd, stack, &combined)) ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);
}

static void transform_restore_top(FluxRenderCtx const *rc, FluxTransformStack *stack) {
	D2D1_MATRIX_3X2_F restored = stack->frames [--stack->top];
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &restored);
	if (stack->clipped [stack->top]) ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void execute_transform_pop(FluxRenderCtx const *root, FluxTransformStack *stack) {
	if (stack->clamped == 0 && stack->top > 0 && stack->projected [stack->top - 1]) {
		FluxProjection *p = &stack->proj [--stack->nproj];
		flux_projection_end(p, stack_rc(stack, root));
	}
	FluxRenderCtx const *rc = stack_rc(stack, root);
	if (stack->clamped > 0) stack->clamped--;
	else if (stack->top > 0) transform_restore_top(rc, stack);
	ID2D1RenderTarget_PopLayer(FLUX_RT(rc));
}

static void execute_draw(FluxEngine const *eng, FluxRenderCtx const *rc, FluxRenderCommand const *cmd) {
	if (cmd->phase == FLUX_PHASE_MAIN) {
		flux_display_paint_main(eng->store, eng->backend, rc, &cmd->snapshot, &cmd->bounds, &cmd->state);
		return;
	}
	flux_display_paint_overlay(eng->backend, rc, &cmd->snapshot, &cmd->bounds, &cmd->state);
}

static void execute_unwind(FluxRenderCtx const *root, FluxTransformStack *stack) {
	while (stack->nproj) {
		FluxProjection *p = &stack->proj [--stack->nproj];
		flux_projection_end(p, stack_rc(stack, root));
	}
}

static bool cmd_intersects_clip(FluxRenderCommand const *cmd, FluxRect const *clip) {
	if (!clip || clip->w <= 0.0f || clip->h <= 0.0f) return true;
	float const pad = 4.0f;
	if (cmd->bounds.x + cmd->bounds.w + pad < clip->x) return false;
	if (cmd->bounds.y + cmd->bounds.h + pad < clip->y) return false;
	if (cmd->bounds.x - pad > clip->x + clip->w) return false;
	return cmd->bounds.y - pad <= clip->y + clip->h;
}

void flux_engine_execute_clipped(FluxEngine const *eng, FluxRenderCtx const *rc, FluxRect const *clip) {
	if (!eng || !rc) return;

	FluxTransformStack stack = {0};
	FluxEngine        *mut   = ( FluxEngine * ) eng;

	for (uint32_t i = 0; i < eng->commands.count; i++) {
		FluxRenderCommand const *cmd    = &eng->commands.cmds [i];
		FluxRenderCtx const     *active = stack_rc(&stack, rc);
		if (cmd->clip_action == FLUX_CLIP_PUSH) {
			execute_clip_push(mut, active, cmd, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_POP) {
			execute_clip_pop(active, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_PUSH_TRANSFORM) {
			execute_transform_push(mut, active, cmd, &stack);
			continue;
		}
		if (cmd->clip_action == FLUX_CLIP_POP_TRANSFORM) {
			execute_transform_pop(rc, &stack);
			continue;
		}
		if (stack.top == 0u && !cmd_intersects_clip(cmd, clip)) continue;
		execute_draw(eng, active, cmd);
	}
	execute_unwind(rc, &stack);
}

void flux_engine_execute(FluxEngine const *eng, FluxRenderCtx const *rc) { flux_engine_execute_clipped(eng, rc, NULL); }
