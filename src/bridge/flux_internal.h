/**
 * @file flux_internal.h
 * @brief Fluxent backend internals: backend vtable implementation, binding, event trampolines.
 *
 * The reconciler and runtime now live in xent-kit (xtk_reconcile.h). This
 * header covers the fluxent-specific backend, dispatch tables, and glue.
 */
#ifndef FLUX_INTERNAL_H
#define FLUX_INTERNAL_H

#include "fluxent/fluxent.h"
#include <xtk/xtk_reconcile.h>
#include <stddef.h>

typedef struct FluxBinding FluxBinding;

typedef struct FluxBackendCtx {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxApp       *app;
	XtkRuntime    *runtime;
} FluxBackendCtx;

typedef struct FluxNodeExt {
	FluxMenuFlyout *menu;
	FluxBinding      *menu_bindings;
	int             menu_binding_count;
	XentNodeId     *tab_hosts;
	int             tab_host_count;
} FluxNodeExt;

struct FluxBinding {
	XtkRuntime *rt;
	XtkMsg      on_click;
	XtkMsg      on_change;
	XtkMsg      on_close;
};

XtkBackend flux_xtk_backend(FluxBackendCtx *bctx);

XentNodeId flux_create_control(FluxBackendCtx *rt, XentNodeId parent, XtkEl const *el, FluxBinding *b);
void       flux_apply_props(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el);
bool       flux_is_interactive(XtkEl const *el);
void       flux_mount_setup(FluxBackendCtx *rt, XtkNode *n, XentNodeId id, XtkEl const *el);

void flux_tramp_click(void *ud);
void flux_tramp_titlebar_pane(void *ud);
void flux_tramp_toggle(void *ud, FluxCheckState state);
void flux_tramp_slider(void *ud, float value);
void flux_tramp_text(void *ud, char const *text);
void flux_tramp_password(void *ud, char const *text);
void flux_tramp_select(void *ud, int index);
void flux_tramp_invoke(void *ud, int index);
void flux_tramp_query(void *ud, char const *text);
void flux_tramp_chosen(void *ud, int index);
void flux_tramp_rating(void *ud, double value, int stars);
void flux_tramp_expand(void *ud, bool expanded);
void flux_tramp_double(void *ud, double value);
bool flux_tramp_tab_close_req(void *ud, int index);
void flux_tramp_dialog_result(void *ud, FluxDialogResult result);
void flux_tramp_tip_close(void *ud, FluxTeachingTipCloseReason reason);
void flux_tramp_refresh(void *ud, int direction);
void flux_tramp_tree_invoke(void *ud, int flat_index);
void flux_tramp_tree_expand(void *ud, int flat_index, bool expanded);
void flux_tramp_tree_select(void *ud, int flat_index, bool selected);

FluxWindow       *flux_be_window(FluxBackendCtx *rt);
FluxTextRenderer *flux_be_text(FluxBackendCtx *rt);
FluxThemeManager *flux_be_theme(FluxBackendCtx *rt);

void flux_be_fill_menu(FluxBackendCtx *rt, FluxMenuFlyout *menu, XtkMenuItemDesc const *items, int count, FluxBinding *slots);

#endif
