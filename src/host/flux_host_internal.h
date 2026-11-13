#ifndef FLUX_HOST_INTERNAL_H
#define FLUX_HOST_INTERNAL_H

#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>
#include <xtk/xtk_alloc.h>
#include <xtk/xtk_comp_rt.h>
#include <xtk/xtk_host.h>

#include "fluxent/flux_node_store.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_control_build.h"
#include "fluxent/flux_host.h"

#include <string.h>
#include <xent/xent.h>

typedef struct PropEntry {
	XtkCtrl        control;
	XtkPropertyId  id;
	XtkValue       value;
	XtkChangeStamp change;
	bool           live;
} PropEntry;

typedef struct SubEntry {
	XtkSubId   id;
	XtkCtrl    target;
	XtkEventId event;
	uint32_t   phases;
	XtkEventFn fn;
	void      *userdata;
	bool       live;
} SubEntry;

typedef struct FluxHostState {
	XtkHost        host;
	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     root;
	bool           owns_tree;
	uint64_t       revision;
	XtkCtrlId     *types;
	uint32_t       types_cap;
	uint32_t      *invalidate;
	uint32_t       invalidate_cap;
	uint32_t       pending_invalidation;
	XtkCtrl        focus;
	PropEntry     *props;
	uint32_t       prop_count;
	uint32_t       prop_cap;
	SubEntry      *subs;
	uint32_t       sub_count;
	uint32_t       sub_cap;
	uint64_t       next_sub;
	XtkCompRt     *comp;
	FluxControlEnv env;
} FluxHostState;

typedef enum UndoKind
{
	UNDO_CREATE = 1,
	UNDO_PROP,
	UNDO_TOPO,
	UNDO_SUB,
} UndoKind;

typedef enum SubUndoKind
{
	SUB_UNDO_CANCEL_ADD = 0,
	SUB_UNDO_RESTORE,
} SubUndoKind;

typedef struct UndoTopo {
	XentNodeId child;
	XentNodeId old_parent;
	XentNodeId old_next;
} UndoTopo;

typedef struct UndoProp {
	XtkCtrl        control;
	XtkPropertyId  id;
	XtkValue       old_value;
	XtkChangeStamp old_change;
	bool           had_value;
} UndoProp;

typedef struct UndoSub {
	XtkSubId    id;
	SubUndoKind kind;
} UndoSub;

typedef struct Undo {
	UndoKind kind;

	union
	{
		XentNodeId created;
		UndoProp   prop;
		UndoTopo   topo;
		UndoSub    sub;
	} u;
} Undo;

typedef struct PendingInvalidation {
	XtkCtrl  control;
	uint32_t mask;
} PendingInvalidation;

enum
{
	FLUX_HOST_INLINE_INVALIDATIONS = 8,
};

typedef struct Prepare {
	FluxHostState       *R;
	XtkBatch            *batch;
	XtkCtrl             *locals;
	uint32_t             local_count;
	Undo                *undos;
	uint32_t             undo_count;
	uint32_t             undo_cap;
	XentNodeId          *deferred_destroy;
	uint32_t             destroy_count;
	uint32_t             destroy_cap;
	XtkSubId            *sub_ids;
	uint32_t             sub_id_count;
	PendingInvalidation *invalidations;
	uint32_t             invalidation_count;
	uint32_t             invalidation_cap;
	PendingInvalidation  invalidation_inline [FLUX_HOST_INLINE_INVALIDATIONS];
} Prepare;

static XentNodeId inline flux_host_as_node(XtkCtrl c) { return ( XentNodeId ) c; }

static XtkCtrl inline flux_host_as_control(XentNodeId n) { return ( XtkCtrl ) n; }

static FluxHostState inline *flux_host_state(XtkHost *host) { return host ? ( FluxHostState * ) host->impl : NULL; }

static FluxHostState const inline *flux_host_state_c(XtkHost const *host) {
	return host ? ( FluxHostState const * ) host->impl : NULL;
}

static bool inline flux_host_node_live(FluxHostState const *R, XtkCtrl control) {
	return control != XTK_CTRL_INVALID && xent_node_valid(R->ctx, flux_host_as_node(control));
}

void             flux_host_value_clear(XtkValue *v);
bool             flux_host_value_clone(XtkValue *dst, XtkValue const *src);
PropEntry       *flux_host_prop_find(FluxHostState *R, XtkCtrl control, XtkPropertyId id);
PropEntry const *flux_host_prop_findc(FluxHostState const *R, XtkCtrl control, XtkPropertyId id);
PropEntry       *flux_host_prop_add(FluxHostState *R);
void             flux_host_clear_props(FluxHostState *R, XtkCtrl control);
XtkBatchError    flux_host_apply_prop(Prepare *P, XtkCtrl control, XtkPropPatch const *patch);
void             flux_host_rollback_prop(FluxHostState *R, UndoProp *prop);
bool             flux_host_value_view(XtkValue const *v, XtkValueView *out);
bool             flux_host_can_project(XtkCtrlId type, XtkPropPatch const *patch);
bool             flux_host_apply_native(FluxHostState *R, XtkCtrl control, XtkPropPatch const *patch);

void             flux_host_clear_subs(FluxHostState *R, XtkCtrl control);
XtkBatchError    flux_host_apply_subscribe(Prepare *P, XtkBatchSubscribe const *op);
XtkBatchError    flux_host_apply_unsubscribe(Prepare *P, XtkSubId id);
void             flux_host_rollback_sub(FluxHostState *R, UndoSub const *sub);
uint32_t         flux_host_dispatch_at(FluxHostState *R, XtkEvent *event);

bool             flux_host_types_set(FluxHostState *R, XentNodeId node, XtkCtrlId type);
XtkCtrlId        flux_host_types_get(FluxHostState const *R, XentNodeId node);
bool             flux_host_note_invalidate(FluxHostState *R, XtkCtrl control, uint32_t mask);
bool             flux_host_stage_invalidate(Prepare *P, XtkCtrl control, uint32_t mask);
bool             flux_host_stage_prop_invalidation(Prepare *P, XtkCtrl control, XtkPropertyId id);
bool             flux_host_stage_lifecycle(Prepare *P, XtkCtrl control, bool create);
bool             flux_host_prepare_invalidations(Prepare *P);
void             flux_host_publish_invalidations(Prepare *P);
void             flux_host_purge_sidecar(FluxHostState *R, XentNodeId root);

bool             flux_host_undo_push(Prepare *P, Undo u);
bool             flux_host_defer_destroy(Prepare *P, XentNodeId node);
bool             flux_host_resolve_ref(Prepare const *P, XtkNodeRef ref, bool *ok, XtkCtrl *out);
void             flux_host_rollback(Prepare *P);
void             flux_host_prepare_free(Prepare *P);

XtkBatchError    flux_host_preflight(XtkHost *host, XtkBatch *batch, XtkBatchDiag *diagnostic);
XtkBatchError    flux_host_prepare(XtkHost *host, XtkBatch *batch, XtkPrepare **out_prepare, XtkBatchDiag *diagnostic);
XtkBatchResult  *flux_host_commit(XtkHost *host, XtkPrepare *prepare, XtkBatchDiag *diagnostic);
void             flux_host_abort(XtkHost *host, XtkPrepare *prepare);

bool             flux_host_is_live(XtkHost const *host, XtkCtrl control);
uint64_t         flux_host_revision(XtkHost const *host);
XtkCtrl          flux_host_scene_root(XtkHost const *host);
XtkCtrlId        flux_host_control_type(XtkHost const *host, XtkCtrl control);
XtkCtrl          flux_host_parent(XtkHost const *host, XtkCtrl control);
XtkCtrl          flux_host_first_child(XtkHost const *host, XtkCtrl control);
XtkCtrl          flux_host_next_sibling(XtkHost const *host, XtkCtrl control);
XtkCtrlDesc const *flux_host_descriptor(XtkHost const *host, XtkCtrlId type);
bool               flux_host_get_property(
  XtkHost const *host, XtkCtrl control, XtkPropertyId property, XtkValueView *out_value, XtkChangeStamp *out_change
);
bool                       flux_host_res_valid(XtkHost const *host, XtkResource resource, XtkResourceTypeId type);
bool                       flux_host_resource_retain(XtkHost *host, XtkResource resource, XtkResourceTypeId type);
void                       flux_host_resource_release(XtkHost *host, XtkResource resource, XtkResourceTypeId type);
uint32_t                   flux_host_dispatch(XtkHost *host, XtkEvent const *event);

extern XtkHostVtable const flux_host_vtable;

#endif
