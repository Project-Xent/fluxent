#ifndef FLUX_NODE_DATA_H
#define FLUX_NODE_DATA_H

#include "fluxent/flux_popup.h"
#include <fluxent/flux_types.h>
#include "store/flux_node_data.h"
#include <fluxent/flux_control_type.h>

#include <xent/xent.h>
#include <xent/xent_display.h>

#include <stdbool.h>
#include <stdint.h>

typedef enum FluxDialogSurface
{
	FLUX_DIALOG_SURFACE_NONE,
	FLUX_DIALOG_SURFACE_SMOKE,
	FLUX_DIALOG_SURFACE_CARD,
} FluxDialogSurface;

typedef struct FluxNodeVisuals {
	FluxColor         background;
	FluxColor         border_color;
	float             corner_radius;
	float             border_width;
	float             opacity;
	FluxDialogSurface dialog_surface;
} FluxNodeVisuals;

typedef struct FluxNodeState {
	uint8_t visible       :1;
	uint8_t hovered       :1;
	uint8_t pressed       :1;
	uint8_t focused       :1;
	uint8_t dirty         :1;
	uint8_t suppress_focus:1;
	uint8_t pointer_type;
} FluxNodeState;

typedef struct FluxNodeBehavior {
	void        (*on_click)(void *ctx);
	void       *on_click_ctx;
	void        (*on_pointer_move)(void *ctx, float x, float y);
	void       *on_pointer_move_ctx;
	void        (*on_pointer_down)(void *ctx, float x, float y, int click_count);
	void       *on_pointer_down_ctx;
	void        (*on_focus)(void *ctx);
	void       *on_focus_ctx;
	void        (*on_blur)(void *ctx);
	void       *on_blur_ctx;
	bool        (*on_tab)(void *ctx, bool shift, bool entering);
	void       *on_tab_ctx;
	XentNodeId  (*tab_entry)(void *ctx, XentNodeId group);
	void       *tab_entry_ctx;
	bool        (*on_key)(void *ctx, FluxKey key, bool down);
	void       *on_key_ctx;
	void        (*on_char)(void *ctx, wchar_t ch);
	void       *on_char_ctx;
	void        (*on_ime_composition)(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
	void       *on_ime_composition_ctx;
	void        (*on_context_menu)(void *ctx, float x, float y);
	void       *on_context_menu_ctx;
	void        (*on_cancel)(void *ctx);
	void       *on_cancel_ctx;
	bool        (*on_release)(void *ctx);
	void       *on_release_ctx;
	void        (*on_middle_click)(void *ctx);
	void       *on_middle_click_ctx;
	void        (*on_hover_changed)(void *ctx, bool hovered);
	void       *on_hover_changed_ctx;
	bool        (*auto_tooltip)(void *ctx, char *text, size_t cap, FluxRect *local, FluxPlacement *placement);
	void       *auto_tooltip_ctx;
	char const *(*tooltip_at)(void *ctx, float local_x, float local_y);
	void       *tooltip_at_ctx;
	bool        (*chrome_hit)(void *ctx, float local_x, float local_y);
	void       *chrome_hit_ctx;
} FluxNodeBehavior;

typedef struct FluxPrevRender {
	float scale_x;
	float scale_y;
	float opacity;
	float translate_x;
	float translate_y;
	float rotate_x;
	float rotate_y;
	float rotate_z;
	float pivot_x;
	float pivot_y;
	bool  clip_subtree;
} FluxPrevRender;

typedef struct FluxNodeData {
	XentNodeId                 node_id;
	FluxNodeVisuals            visuals;
	FluxNodeState              state;
	FluxNodeBehavior           behavior;
	FluxControlType            component_type;
	void                      *component_data;
	struct FluxUiaHooks const *uia;
	struct FluxPopup          *popup;
	XentNodeId                 uia_popup;
	XentNodeId                 uia_owner;
	XentNodeId                 uia_graft;
	XentNodeId                 uia_adopter;
	int                        uia_part;
	struct FluxUiaProps       *a11y;
	bool                       focus_passive;
	bool                       tab_reverse_children;
	bool                       tab_inert;
	bool                       tab_cycle;
	void                       (*destroy_component_data)(void *component_data);

	float                      hover_local_x;
	float                      hover_local_y;

	char const                *tooltip_text;
	uint8_t                    nav_slot;
	uint8_t                    drop_operation;
	char const                *drag_text;
	char const                *drag_files;

	float                      render_scale;
	float                      render_scale_x;
	float                      render_scale_y;
	float                      render_opacity;
	float                      render_translate_x;
	float                      render_translate_y;
	float                      render_rotate_x;
	float                      render_rotate_y;
	float                      render_rotate_z;
	float                      render_pivot_x;
	float                      render_pivot_y;
	float                      motion_translate_x;
	float                      motion_translate_y;
	float                      motion_opacity;
	bool                       retiring;
	bool                       connected_hidden;
	struct FluxMotionState    *motion;
	bool                       render_clip_subtree;
	float                      clip_top;
	bool                       clips_children;

	XentDl const              *display_list;
	bool                       display_list_owned;
	uint64_t                   dl_hash;
	float                      dl_w;
	float                      dl_h;
	uint8_t                    dl_dynamic;
	bool                       has_prev_bounds;
	FluxRect                   prev_bounds;
	uint64_t                   collect_hash;
	float                      prev_scroll_x;
	float                      prev_scroll_y;
	bool                       has_prev_render;
	FluxPrevRender             prev_render;
} FluxNodeData;

struct FluxStore;
typedef struct FluxStore FluxStore;

FluxNodeData            *flux_store_find(FluxStore *store, XentNodeId id);
FluxNodeData            *flux_store_ensure(FluxStore *store, XentNodeId id);

bool                     flux_node_lift(FluxStore *store, XentNodeId node, bool lifted);
bool                     flux_node_lifted(FluxStore const *store, XentNodeId node);
uint32_t                 flux_store_nlifted(FluxStore const *store);
XentNodeId               flux_store_lifted(FluxStore const *store, uint32_t i);
void                     flux_store_track_frame(FluxStore *store, XentNodeId node, FluxControlType type);
uint32_t                 flux_store_nframe(FluxStore const *store);
XentNodeId               flux_store_frame(FluxStore const *store, uint32_t i);

struct FluxMotionSystem;
void                     flux_store_setmotion(FluxStore *store, struct FluxMotionSystem *motion);
struct FluxMotionSystem *flux_store_motion(FluxStore const *store);

typedef struct FluxUiaNote {
	XentNodeId node;
	int        sub;
	int        event;
	int        kind;
	int        processing;
	char      *text;
	char      *activity;
	double     old_v;
	double     new_v;
} FluxUiaNote;

void flux_uia_note(FluxStore *store, XentNodeId node, int sub, int event);
void flux_uia_retarget_notes(FluxStore *store, XentNodeId from, XentNodeId to);
void flux_uia_noteprop(FluxStore *store, XentNodeId node, int property, double old_v, double new_v);
void flux_uia_notify(
  FluxStore *store, XentNodeId node, int kind, int processing, char const *text, char const *activity
);
uint32_t           flux_store_nnotes(FluxStore const *store);
FluxUiaNote const *flux_store_note(FluxStore const *store, uint32_t i);
void               flux_store_clearnotes(FluxStore *store);

enum
{
	FLUX_UIA_DIRTY_PROPS   = 1u << 0,
	FLUX_UIA_DIRTY_PATTERN = 1u << 1,
	FLUX_UIA_DIRTY_TREE    = 1u << 2,
	FLUX_UIA_DIRTY_FOCUS   = 1u << 3,
};

typedef struct FluxUiaDirtyItem {
	XentNodeId node;
	uint8_t    flags;
} FluxUiaDirtyItem;

void     flux_uia_mark_dirty(FluxStore *store, XentNodeId node, uint8_t flags);
void     flux_uia_mark_all_dirty(FluxStore *store);
void     flux_uia_clear_dirty(FluxStore *store);
uint32_t flux_uia_drain_dirty(FluxStore *store, FluxUiaDirtyItem **out_buf, uint32_t *out_cap, bool *out_all);

static void inline flux_node_settype(FluxStore *store, XentNodeId node, FluxControlType type) {
	FluxNodeData *nd = flux_store_ensure(store, node);
	if (!nd) return;
	nd->component_type = type;
	flux_store_track_frame(store, node, type);
}

static FluxControlType inline flux_node_type(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd ? nd->component_type : FLUX_CONTROL_CONTAINER;
}

#endif
