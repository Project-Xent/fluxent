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

typedef struct FluxNodeVisuals {
	FluxColor background;
	FluxColor border_color;
	float     corner_radius;
	float     border_width;
	float     opacity;
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

typedef struct FluxNodeData {
	XentNodeId       node_id;
	FluxNodeVisuals  visuals;
	FluxNodeState    state;
	FluxNodeBehavior behavior;
	FluxControlType  component_type;
	void            *component_data;
	void             (*destroy_component_data)(void *component_data);

	float            hover_local_x;
	float            hover_local_y;

	char const      *tooltip_text;

	float            render_scale;
	float            render_opacity;
	float            render_translate_x;
	float            render_translate_y;
	bool             render_clip_subtree;
	bool             clips_children;

	XentDl const    *display_list;
	bool             display_list_owned;
} FluxNodeData;

struct FluxStore;
typedef struct FluxStore FluxStore;

FluxNodeData            *flux_store_find(FluxStore *store, XentNodeId id);
FluxNodeData            *flux_store_ensure(FluxStore *store, XentNodeId id);

static void inline flux_node_settype(FluxStore *store, XentNodeId node, FluxControlType type) {
	FluxNodeData *nd = flux_store_ensure(store, node);
	if (nd) nd->component_type = type;
}

static FluxControlType inline flux_node_type(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd ? nd->component_type : FLUX_CONTROL_CONTAINER;
}

#endif
