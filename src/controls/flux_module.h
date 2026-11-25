#ifndef FLUX_MODULE_H
#define FLUX_MODULE_H

#include "fluxent/flux_controls.h"
#include "controls/factory/flux_control_build.h"
#include "render/flux_engine.h"
#include "fluxent/flux_text.h"

#include <xent/xent_display.h>
#include <xtk/xtk_batch.h>

#include <wchar.h>

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
	XentNodeId    (*slot_value)(FluxStore *store, XentNodeId node, uint32_t slot);
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

struct FluxUiaTextView;

typedef struct FluxUiaSub {
	int        (*kind)(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
	bool       (*span)(FluxStore *store, XentNodeId node, int *first, int *last);
	bool       (*rect)(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
	void       (*name)(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
	bool       (*focused)(FluxStore *store, XentNodeId node, int sub);
	bool       (*focusable)(FluxStore *store, XentNodeId node, int sub);
	bool       (*invoke)(FluxStore *store, XentNodeId node, int sub);
	bool       (*selected)(FluxStore *store, XentNodeId node, int sub);
	bool       (*select)(FluxStore *store, XentNodeId node, int sub, int op);
	int        (*heads)(FluxStore *store, XentNodeId node);
	bool       (*grid)(FluxStore *store, XentNodeId node, int *rows, int *cols);
	bool       (*cell)(FluxStore *store, XentNodeId node, int sub, int *row, int *col);
	int        (*row_head)(FluxStore *store, XentNodeId node, int sub);
	void       (*scroll)(FluxStore *store, XentNodeId node, int sub);
	int        (*count)(FluxStore *store, XentNodeId node);
	int        (*index)(FluxStore *store, XentNodeId node, XentNodeId child);
	XentNodeId (*realized)(FluxStore *store, XentNodeId node, int sub);
	void       (*realize)(FluxStore *store, XentNodeId node, int sub);
	void       (*focus)(FluxStore *store, XentNodeId node, int sub);
	bool       (*setinfo)(FluxStore *store, XentNodeId node, int sub, int *pos, int *size, int *level);
	bool       (*enabled)(FluxStore *store, XentNodeId node, int sub);
	bool       (*text)(FluxStore *store, XentNodeId node, int sub, struct FluxUiaTextView *out);
	int        column_head;
} FluxUiaSub;

typedef struct FluxUiaTextView {
	FluxText     *renderer;
	char const   *text;
	FluxTextStyle style;
	float         layout_w;
	float         origin_x;
	float         origin_y;
	XentRect      clip;
	uint32_t      sel_start;
	uint32_t      sel_end;
	bool          selectable;
	bool          readonly;
	bool          paragraphs;
} FluxUiaTextView;

typedef struct FluxUiaText {
	bool (*view)(FluxStore *store, XentNodeId node, FluxUiaTextView *out);
	void (*select)(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end);
	void (*reveal)(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end);
} FluxUiaText;

typedef struct FluxUiaHooks {
	int                role;
	unsigned           patterns;
	int                (*expand)(FluxStore *store, XentNodeId node);
	void               (*toggle_expand)(FluxStore *store, XentNodeId node);
	bool               (*selected)(FluxStore *store, XentNodeId node);
	bool               (*select)(FluxStore *store, XentNodeId node, int op);
	unsigned           (*select_mode)(FluxStore *store, XentNodeId node);
	void               (*close)(FluxStore *store, XentNodeId node);
	unsigned           (*live)(FluxStore *store, XentNodeId node);
	FluxUiaSub const  *sub;
	void               (*name)(FluxStore *store, XentNodeId node, char *out, size_t size);
	XentNodeId         (*scroller)(FluxStore *store, XentNodeId node);
	FluxUiaText const *doc;
	bool               (*scroll_info)(FluxStore *store, XentNodeId node, double *percent, double *view);
	void               (*scroll_by)(FluxStore *store, XentNodeId node, int items);
	bool               (*scroll_set)(FluxStore *store, XentNodeId node, double percent);
	bool               modal;
	bool               dialog;
	bool               invoked;
	int                landmark;
	wchar_t const     *klass;
	wchar_t const     *lctype;
	int                (*orient)(FluxStore *store, XentNodeId node);
	bool               (*setinfo)(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
	bool               (*concealed)(FluxStore *store, XentNodeId node);
	int                (*toggle_state)(FluxStore *store, XentNodeId node);
	void               (*toggle)(FluxStore *store, XentNodeId node);
	bool               (*invoke)(FluxStore *store, XentNodeId node);
	bool               (*admits)(FluxStore *store, XentNodeId node, XentNodeId child);
	void               (*steps)(FluxStore *store, XentNodeId node, double *small_step, double *large_step);
	XentNodeId         (*delegate)(FluxStore *store, XentNodeId node);
	XentNodeId         (*controls)(FluxStore *store, XentNodeId node);
	bool               unlisted;
	bool               drop_flat;
	bool               select_shared;
	wchar_t const     *lmtype;
	wchar_t const     *help;
	bool               (*range)(FluxStore *store, XentNodeId node, double *value, double *lo, double *hi);
	bool               (*readonly)(FluxStore *store, XentNodeId node);
	char const        *(*text)(FluxStore *store, XentNodeId node);
	void               (*set_text)(FluxStore *store, XentNodeId node, char const *text);
	void               (*set_range)(FluxStore *store, XentNodeId node, double value);
	bool               (*thumb)(FluxStore *store, XentNodeId node, int thumb, float *value, float *lo, float *hi);
	bool               (*set_thumb)(FluxStore *store, XentNodeId node, int thumb, float value);
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
