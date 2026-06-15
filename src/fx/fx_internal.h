/**
 * @file fx_internal.h
 * @brief FX internals: frame arena, build context, shadow tree, and runtime
 * shared by the builder, reconciler, and app-loop translation units.
 */
#ifndef FX_INTERNAL_H
#define FX_INTERNAL_H

#include "fluxent/fluxent.h"
#include "fluxent/fx.h"
#include <stddef.h>

/** @brief Chunked bump allocator; reset keeps the first chunk hot. */
typedef struct FxArenaChunk {
	struct FxArenaChunk *next;
	size_t               capacity;
	size_t               used;
	_Alignas(max_align_t) unsigned char data [];
} FxArenaChunk;

typedef struct FxArena {
	FxArenaChunk *head;  /**< Current chunk (allocation cursor). */
	FxArenaChunk *first; /**< Retained across resets. */
} FxArena;

bool  fx_arena_init(FxArena *arena, size_t first_chunk_size);
void  fx_arena_dispose(FxArena *arena);
void  fx_arena_reset(FxArena *arena);
void *fx_arena_alloc(FxArena *arena, size_t size);

/* Two frame arenas alternate so the previous frame's element tree stays
 * alive while the reconciler diffs the new one against it. */
struct FxUi {
	FxArena arenas [2];
	int     current;
};

FxArena                 *fx_ui_arena(FxUi *ui);

typedef struct FxRuntime FxRuntime;

/**
 * @brief Stable per-control event binding. Retained-control callbacks point
 * here for the node's whole life; the reconciler only rewrites the message
 * values, never the callback wiring.
 */
typedef struct FxBinding {
	FxRuntime *rt;
	FxMsg      on_click;
	FxMsg      on_change;
	FxMsg      on_close; /**< Third channel: tab close requests. */
} FxBinding;

/** @brief Shadow node: maps one mounted element onto its retained control. */
typedef struct FxNode {
	XentNodeId      node;
	XentNodeId      host;          /**< Where children mount: == node except for
	                                * content-host controls (nav view, expander,
	                                * tab view, content dialog). */
	FxBinding      *binding;       /**< Heap; NULL for non-interactive controls. */
	FluxMenuFlyout *menu;          /**< Owned declarative menu (dropdown/split button). */
	FxBinding      *menu_bindings; /**< One per menu item; stable while the menu lives. */
	int             menu_binding_count;
	XentNodeId     *tab_hosts;     /**< Tab view: per-tab content nodes; host tracks the selection. */
	int             tab_host_count;
	struct FxNode **children;
	int             child_count;
	int             child_cap;
} FxNode;

#define FX_QUEUE_CAP 256

struct FxRuntime {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     host; /**< Parent the root element mounts under. */
	FluxApp       *app;  /**< NULL in headless tests. */

	FxUi          *ui;
	void          *model;
	void           (*update)(void *model, FxMsg msg);
	FluxEl        *(*view)(FxUi *ui, void *model);

	FluxEl        *prev_root;
	FxNode        *root;

	FxMsg          queue [FX_QUEUE_CAP];
	int            q_head;
	int            q_tail;
};

/** @brief Allocate a new element on the frame arena (NAN sizes, zeroed). */
FluxEl            *fx_el_new(FxUi *ui, FluxControlType type, char const *text);

/** @brief Adopt an FX_END-terminated child array (skips NULLs, dense copy). */
void               fx_el_adopt(FxUi *ui, FluxEl *el, FluxEl *children []);

/** @brief Copy a declarative menu-item array into the frame arena. */
FxMenuItemDesc const *fx_menu_items_dup(FxUi *ui, FxMenuItemDesc const *items, int count);

/** @brief Diff @p next against the runtime's previous frame and apply. */
void       fx_reconcile(FxRuntime *rt, FluxEl *next);

/** @brief Unmount the whole mounted tree (runtime teardown). */
void       fx_unmount_all(FxRuntime *rt);

/** @brief Apply per-type + common properties; dispatches through a table. */
void       fx_apply_props(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el);

/** @brief Create the retained control for @p el under @p parent. */
XentNodeId fx_create_control(FxRuntime *rt, XentNodeId parent, FluxEl const *el, FxBinding *b);

/** @brief True for control types that need an FxBinding (event wiring). */
bool       fx_el_interactive(FluxEl const *el);

/** @brief Post-create mount setup: content hosts, tab tabs, menu-bar menus. */
void       fx_mount_setup(FxRuntime *rt, FxNode *n, XentNodeId id, FluxEl const *el);

/* Event trampolines: retained callbacks -> FxMsg posts. */
void fx_tramp_click(void *ud);
void fx_tramp_toggle(void *ud, FluxCheckState state);
void fx_tramp_slider(void *ud, float value);
void fx_tramp_text(void *ud, char const *text);
void fx_tramp_select(void *ud, int index);
void fx_tramp_expand(void *ud, bool expanded);
void fx_tramp_double(void *ud, double value);
bool fx_tramp_tab_close_req(void *ud, int index);
void fx_tramp_dialog_result(void *ud, FluxDialogResult result);

/* App-owned service accessors. */
FluxWindow       *fx_rt_window(FxRuntime *rt);
FluxTextRenderer *fx_rt_text(FxRuntime *rt);
FluxThemeManager *fx_rt_theme(FxRuntime *rt);

/* Fill a menu flyout from declarative item descriptors. */
void fx_fill_menu(FxRuntime *rt, FluxMenuFlyout *menu, FxMenuItemDesc const *items, int count, FxBinding *slots);

/** @brief Enqueue a message and request a frame (drops when full). */
void       fx_runtime_post(FxRuntime *rt, FxMsg msg);

/** @brief Create a runtime over an existing scene (app optional/headless). */
FxRuntime *fx_runtime_create(
  XentContext *ctx, FluxNodeStore *store, XentNodeId host, FluxApp *app, void *model, void (*update)(void *, FxMsg),
  FluxEl *(*view)( FxUi *, void * )
);

/** @brief Unmount, then free the runtime and its arenas. */
void fx_runtime_destroy(FxRuntime *rt);

/** @brief One frame pump: drain messages -> update -> view -> reconcile. */
void fx_runtime_frame(FxRuntime *rt);

#endif
