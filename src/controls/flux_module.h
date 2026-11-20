#ifndef FLUX_MODULE_H
#define FLUX_MODULE_H

#include "fluxent/flux_controls.h"
#include "controls/factory/flux_control_build.h"
#include "render/flux_engine.h"

#include <xent/xent_display.h>
#include <xtk/xtk_batch.h>

typedef struct FluxHostState FluxHostState;
typedef struct FluxSnapCtx   FluxSnapCtx;
typedef struct FluxTouch     FluxTouch;

typedef XentDl              *(*FluxControlRecordFn)(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *local_bounds, FluxControlState const *state
);
typedef XentNodeId (*FluxSeedFn)(XentCtx *ctx, FluxStore *store, FluxControlMount const *mount);
typedef void       (*FluxRetargetFn)(XentCtx *ctx, FluxStore *store, XentNodeId node);
typedef void       (*FluxLayoutFn)(XentCtx *ctx, XentNodeId node);
typedef bool       (*FluxShellFn)(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlEnv const *env);
typedef void       (*FluxNodeFn)(FluxStore *store, XentNodeId node);
typedef XentNodeId (*FluxContentFn)(FluxStore *store, XentNodeId node);
typedef void       (*FluxSnapFn)(FluxSnapCtx const *ctx);
typedef bool       (*FluxPropFn)(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkValue const *v);
typedef uint32_t   (*FluxFrameFn)(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd);
typedef void       (*FluxTouchFn)(FluxTouch *touch, XentNodeId node);

enum
{
	FLUX_FRAME_DEFER  = 1u << 0,
	FLUX_FRAME_REDRAW = 1u << 1,
};

typedef struct FluxPropHandler {
	XtkPropertyId id;
	XtkValueKind  kind;
	FluxPropFn    fn;
} FluxPropHandler;

typedef struct FluxTreeHooks {
	FluxContentFn content;
	XentNodeId    (*slot)(FluxStore *store, XentNodeId node, XentNodeId child);
	FluxNodeFn    placed;
	FluxTouchFn   touch;
	FluxNodeFn    commit;
	FluxFrameFn   frame;
	FluxNodeFn    deferred;
	FluxNodeFn    focus;
	FluxFrameFn   collect;
} FluxTreeHooks;

typedef struct FluxInputHooks {
	bool       (*wheel)(FluxStore *store, XentNodeId node, float delta, float *remaining);
	bool       (*scroll_wheel)(FluxStore *store, XentNodeId node, float steps);
	bool       (*pull)(FluxStore *store, XentNodeId node, float x, float y, float max_x, float max_y);
	FluxNodeFn release;
	FluxNodeFn cancel;
} FluxInputHooks;

typedef struct FluxUiaHooks {
	int         role;
	unsigned    patterns;
	int         (*expand)(FluxStore *store, XentNodeId node);
	void        (*toggle_expand)(FluxStore *store, XentNodeId node);
	bool        (*selected)(FluxStore *store, XentNodeId node);
	bool        (*select)(FluxStore *store, XentNodeId node, int op);
	unsigned    (*select_mode)(FluxStore *store, XentNodeId node);
	void        (*close)(FluxStore *store, XentNodeId node);
	unsigned    (*live)(FluxStore *store, XentNodeId node);
	bool        modal;
	char const *(*text)(FluxStore *store, XentNodeId node);
	void        (*set_text)(FluxStore *store, XentNodeId node, char const *text);
	void        (*set_range)(FluxStore *store, XentNodeId node, double value);
	bool        (*thumb)(FluxStore *store, XentNodeId node, int thumb, float *value, float *lo, float *hi);
	bool        (*set_thumb)(FluxStore *store, XentNodeId node, int thumb, float value);
} FluxUiaHooks;

struct FluxTouch {
	FluxStore *store;
	XentCtx   *ctx;
	XentNodeId nodes [128];
	uint32_t   count;
};

struct FluxControlModule {
	FluxControlType                 type;
	FluxControlModule const *const *deps;
	FluxSeedFn                      seed;
	FluxRetargetFn                  retarget;
	FluxLayoutFn                    layout;
	FluxShellFn                     shell;
	FluxNodeFn                      unmount;
	XtkPropertyId const            *consumed;
	FluxPropHandler const          *props;
	FluxControlRecordFn             record;
	FluxControlRecordFn             overlay;
	FluxSnapFn                      snapshot;
	FluxTreeHooks const            *tree;
	FluxInputHooks const           *input;
	FluxUiaHooks const             *uia;
};

FluxControlModule const *flux_module_get(FluxControlType type);
FluxControlModule const *flux_module_of(FluxStore *store, XentNodeId node);
FluxTreeHooks const     *flux_module_tree(FluxStore *store, XentNodeId node);
FluxInputHooks const    *flux_module_input(FluxStore *store, XentNodeId node);
FluxUiaHooks const      *flux_module_uia(FluxStore *store, XentNodeId node);
FluxPropHandler const   *flux_module_prop(FluxControlType type, XtkPropertyId id, XtkValueKind kind);
bool                     flux_module_consumes(FluxControlType type, XtkPropertyId id);
void                     flux_touch_note(FluxTouch *touch, XentNodeId node);

#endif
