/**
 * @file flux_render_snapshot.h
 * @brief Immutable render command containing all visual state for a control.
 *
 * FluxRenderSnapshot captures a point-in-time snapshot of a control's
 * appearance and state. The engine collects these during tree traversal
 * and dispatches them to renderers during the execute phase.
 */

#ifndef FLUX_RENDER_SNAPSHOT_H
#define FLUX_RENDER_SNAPSHOT_H

#include "flux_component_data.h"
#include "flux_node_store.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Immutable snapshot of a control's visual state. */
typedef struct FluxRenderSnapshot {
	uint64_t         id;            /**< Node ID. */
	XentControlType  type;          /**< Control type. */

	FluxColor        background;    /**< Fill color. */
	FluxColor        border_color;  /**< Border stroke color. */
	float            corner_radius; /**< Corner radius in DIPs. */
	float            border_width;  /**< Border stroke width. */
	float            opacity;       /**< Overall opacity (0..1). */

	char const      *text_content;  /**< Text to display (may be NULL). */
	char const      *placeholder;   /**< Placeholder text (may be NULL). */
	char const      *font_family;   /**< Font family name. */
	float            font_size;     /**< Font size in DIPs. */
	FluxColor        text_color;    /**< Text foreground color. */
	FluxTextAlign    text_alignment;
	FluxTextVAlign   text_vert_alignment;
	FluxFontWeight   font_weight;
	uint8_t          max_lines;
	bool             word_wrap;

	char const      *label;
	char const      *icon_name;
	FluxButtonStyle  button_style;
	bool             is_checked;

	FluxCheckState   check_state;

	float            min_value;
	float            max_value;
	float            current_value;
	float            step;

	float            scroll_x, scroll_y;
	float            scroll_content_w, scroll_content_h;
	FluxScrollBarVis scroll_h_vis;
	FluxScrollBarVis scroll_v_vis;
	uint8_t          scroll_mouse_over;
	float            scroll_mouse_local_x;
	float            scroll_mouse_local_y;
	uint8_t          scroll_drag_axis;
	uint8_t          scroll_v_up_pressed;
	uint8_t          scroll_v_dn_pressed;
	uint8_t          scroll_h_lf_pressed;
	uint8_t          scroll_h_rg_pressed;
	double           scroll_last_activity_time;

	uint32_t         cursor_position;
	uint32_t         selection_start;
	uint32_t         selection_end;
	float            scroll_offset_x;

	wchar_t const   *composition_text;
	uint32_t         composition_length;
	uint32_t         composition_cursor;

	FluxColor        selection_color;
	bool             readonly;
	bool             indeterminate;

	bool             enabled;

	uint8_t          nb_spin_placement; /**< FluxNBSpinPlacement value. */
	bool             nb_up_enabled;
	bool             nb_down_enabled;
	float            hover_local_x; /**< Cursor X within bounds, or -1 when not hovered. */
} FluxRenderSnapshot;

/** @brief Build an immutable render snapshot for one node. */
void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd);

#ifdef __cplusplus
}
#endif

#endif
