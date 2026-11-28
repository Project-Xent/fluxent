#ifndef FLUX_MOTION_INTERNAL_H
#define FLUX_MOTION_INTERNAL_H

#include "fluxent/flux_motion.h"
#include "render/flux_engine.h"
#include "render/flux_render_snapshot.h"
#include "store/flux_node_data.h"
#include "theme/flux_theme_anim.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxMotionTween {
	float  start;
	float  current;
	float  target;
	double start_time;
	double duration;
	float  x1, y1, x2, y2;
	bool   initialized;
	bool   active;
} FluxMotionTween;

typedef struct FluxMotionColorTween {
	FluxColor start;
	FluxColor current;
	FluxColor target;
	double    start_time;
	double    duration;
	bool      initialized;
	bool      active;
} FluxMotionColorTween;

typedef struct FluxFrameAnim {
	bool                  active;
	bool                  is_outgoing;
	bool                  is_back;
	FluxNavTransitionKind kind;
	double                start_time;
	double                duration;
} FluxFrameAnim;

typedef struct FluxThemeEnterAnim {
	bool                    active;
	bool                    is_exit;
	FluxThemeTransitionKind kind;
	uint32_t                stagger_index;
	double                  start_time;
	double                  delay;
	double                  duration;
	float                   from_x;
	float                   from_y;
} FluxThemeEnterAnim;

struct FluxMotionState {
	FluxImplicitCfg         cfg;
	FluxThemeEnterAnim      theme_enter;
	FluxFrameAnim           frame_anim;
	FluxMotionTween         opacity;
	FluxMotionTween         tx;
	FluxMotionTween         ty;
	FluxMotionTween         sx;
	FluxMotionTween         sy;
	FluxMotionTween         rotation;
	FluxMotionTween         corner_radius;
	FluxMotionColorTween    background;
	FluxMotionColorTween    border_color;
	float                   target_opacity;
	float                   target_tx;
	float                   target_ty;
	float                   target_sx;
	float                   target_sy;
	float                   target_rot;
	float                   target_radius;
	FluxColor               target_bg;
	FluxColor               target_border;
	bool                    has_prev_layout;
	float                   prev_x;
	float                   prev_y;
	FluxThemeTransitionKind enter_exit_kind;
	FluxConnectedCfg        connected_cfg;
	char                   *connected_key;
	char                   *coord_key;
};

typedef struct FluxRetiringEntry {
	XentNodeId parent;
	XentNodeId node;
	bool       below_live;
	float      parent_orig_x;
	float      parent_orig_y;
	double     end_time;
} FluxRetiringEntry;

typedef struct FluxConnectedPrepared {
	char            *key;
	FluxRect         src_rect;
	FluxRect         src_raw_rect;
	XentNodeId       ghost_node;
	FluxConnectedCfg cfg;
	double           expire_time;
	float            corner_radius;
} FluxConnectedPrepared;

typedef struct FluxConnectedCoordItem {
	XentNodeId node;
	FluxRect   offset;
} FluxConnectedCoordItem;

typedef struct FluxConnectedFlight {
	char                   *key;
	XentNodeId              dest_node;
	XentNodeId              ghost_node;
	FluxRect                src_rect;
	FluxRect                src_raw_rect;
	FluxConnectedCfg        cfg;
	double                  start_time;
	double                  duration;
	float                   linear_progress;
	float                   progress;
	float                   corner_radius;
	FluxConnectedCoordItem *coords;
	uint32_t                coord_count;
	bool                    coords_resolved;
} FluxConnectedFlight;

struct FluxMotionSystem {
	FluxStore             *store;
	XentCtx               *ctx;
	double                 now;
	bool                   anims_enabled;
	FluxRetiringEntry     *retiring;
	uint32_t               retiring_count;
	uint32_t               retiring_cap;
	FluxConnectedPrepared *prepared;
	uint32_t               prepared_count;
	uint32_t               prepared_cap;
	FluxConnectedFlight   *flights;
	uint32_t               flight_count;
	uint32_t               flight_cap;
};

typedef struct FluxFrameControlData {
	FluxNavTransitionKind transition;
	bool                  is_back;
	uint64_t              page_key;
	bool                  has_initial_page;
	uint64_t              last_page_key;
	XentNodeId            active_child;
	XentNodeId            pending_outgoing;
	FluxNavTransitionKind active_transition;
	bool                  active_back;
} FluxFrameControlData;

struct FluxMotionSystem *flux_motion_system_create(FluxStore *store, XentCtx *ctx);
void                     flux_motion_system_destroy(struct FluxMotionSystem *sys);
struct FluxMotionSystem *flux_motion_ensure_system(FluxStore *store, XentCtx *ctx);
double                   flux_motion_now(struct FluxMotionSystem *sys);
bool                     flux_motion_track_node(FluxStore *store, XentNodeId node);
struct FluxMotionState  *flux_motion_ensure_state(FluxStore *store, XentNodeId node);
void                     flux_motion_state_free(struct FluxMotionState *st);

float                    flux_motion_bezier(float t, float x1, float y1, float x2, float y2);
float                    flux_motion_exp_in(float t, float exponent);
float                    flux_motion_exp_out(float t, float exponent);
float     flux_motion_seg(double elapsed, double begin, double dur, float x1, float y1, float x2, float y2);
float     flux_motion_lerpf(float a, float b, float t);
FluxColor flux_motion_lerp_rgb(FluxColor a, FluxColor b, float t);

void      flux_motion_tween_set(
  FluxMotionTween *tw, float target, double now, double dur, float x1, float y1, float x2, float y2, bool animate
);
void flux_motion_color_set(FluxMotionColorTween *tw, FluxColor target, double now, double dur, bool animate);
bool flux_motion_tween_step(FluxMotionTween *tw, double now);
bool flux_motion_color_step(FluxMotionColorTween *tw, double now);

void flux_motion_sync_implicit(FluxStore *store, XentNodeId node, FluxNodeData *nd, bool snap_only);
bool flux_motion_step_implicit(struct FluxMotionState *st, FluxNodeData *nd, double now, bool animate);
void flux_motion_patch_snap(FluxNodeData const *nd, FluxRenderSnap *snap);

bool flux_motion_try_retire(FluxStore *store, XentCtx *ctx, XentNodeId node, XentNodeId parent);
bool flux_motion_retire_add(
  struct FluxMotionSystem *sys, XentNodeId parent, XentNodeId node, bool below_live, double end_time
);
void flux_motion_retire_drop_parent(struct FluxMotionSystem *sys, XentNodeId parent);
void flux_motion_commit(FluxStore *store, XentCtx *ctx, XentNodeId const *created, uint32_t created_count);
void flux_motion_on_node_removed(FluxStore *store, XentNodeId node);
void flux_motion_mark_subtree_retiring(FluxStore *store, XentCtx *ctx, XentNodeId root);

bool flux_motion_tick_lifecycle(struct FluxMotionSystem *sys, double now, bool animate);
bool flux_motion_step_frame(struct FluxMotionState *st, FluxNodeData *nd, double now, bool animate);
bool flux_motion_tick_connected(struct FluxMotionSystem *sys, double now, bool animate);

void flux_motion_start_frame_pair(
  FluxStore *store, XentCtx *ctx, FluxNodeData *frame_nd, XentNodeId outgoing, XentNodeId incoming
);
void flux_motion_start_theme_enter(FluxStore *store, XentCtx *ctx, XentNodeId node, uint32_t sibling_index);

void flux_motion_set_connected_key(FluxStore *store, XentNodeId node, char const *key, FluxConnectedCfg const *cfg);
void flux_motion_set_connected_coord(FluxStore *store, XentNodeId node, char const *key);
void flux_motion_connected_on_destroy(FluxStore *store, XentCtx *ctx, XentNodeId node);
bool flux_motion_connected_try_retire(FluxStore *store, XentCtx *ctx, XentNodeId node);
void flux_motion_connected_prepare_subtree(FluxStore *store, XentCtx *ctx, XentNodeId root);
void flux_motion_connected_on_commit(FluxStore *store, XentCtx *ctx, XentNodeId const *created, uint32_t created_count);
void flux_motion_connected_drop_node(struct FluxMotionSystem *sys, XentNodeId node);
bool flux_motion_connected_has_ghost(struct FluxMotionSystem *sys, XentNodeId node);

void flux_motion_collect_retiring(FluxStore *store, FluxEngine *eng, XentCtx *ctx, XentNodeId parent, bool above);
void flux_motion_collect_connected(FluxStore *store, FluxEngine *eng, XentCtx *ctx, XentNodeId root);
void flux_motion_paint_shadow(FluxRenderCtx const *rc, FluxRenderCommand const *cmd);

#endif
