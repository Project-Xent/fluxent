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

/**
 * @brief ScrollViewer-private render state.
 *
 * Read only by flux_draw_scroll; grouped here so it stops appearing as dead
 * weight in every other control's snapshot.
 */
typedef struct FluxScrollSnapshot {
	float            x, y;                         /**< Scroll offset (DIPs). */
	float            content_w, content_h;         /**< Scrollable content extent (DIPs). */
	FluxScrollBarVis h_vis, v_vis;                 /**< Per-axis scrollbar visibility policy. */
	uint8_t          mouse_over;                   /**< Pointer is over the scroll surface. */
	float            mouse_local_x, mouse_local_y; /**< Pointer in scroll-local coords. */
	uint8_t          drag_axis;                    /**< Active thumb-drag axis (0=none). */
	uint8_t          v_up_pressed, v_dn_pressed;   /**< Vertical line-button press state. */
	uint8_t          h_lf_pressed, h_rg_pressed;   /**< Horizontal line-button press state. */
	double           last_activity_time;           /**< Timestamp for scrollbar fade-out. */
} FluxScrollSnapshot;

/**
 * @brief Text-edit-private render state (TextBox / PasswordBox / NumberBox).
 *
 * Read only by the text-input renderers; carries caret, selection, and IME
 * composition so they no longer bloat unrelated control snapshots.
 */
typedef struct FluxEditSnapshot {
	uint32_t       cursor_position;                        /**< Caret index (UTF-8 code units). */
	uint32_t       selection_start, selection_end;         /**< Selection range (code units). */
	float          scroll_offset_x;                        /**< Horizontal text scroll (DIPs). */
	wchar_t const *composition_text;                       /**< Active IME composition, or NULL. */
	uint32_t       composition_length, composition_cursor; /**< IME composition extent + caret. */
	FluxColor      selection_color;                        /**< Selection highlight fill. */
	bool           readonly;                               /**< Read-only field (no caret blink). */
} FluxEditSnapshot;

/** @brief Immutable snapshot of a control's visual state. */
typedef struct FluxRenderSnapshot {
	uint64_t           id;            /**< Node ID. */
	FluxControlType    type;          /**< Control type. */

	FluxColor          background;    /**< Fill color. */
	FluxColor          border_color;  /**< Border stroke color. */
	float              corner_radius; /**< Corner radius in DIPs. */
	float              border_width;  /**< Border stroke width. */
	float              opacity;       /**< Overall opacity (0..1). */

	char const        *text_content;  /**< Text to display (may be NULL). */
	char const        *placeholder;   /**< Placeholder text (may be NULL). */
	char const        *font_family;   /**< Font family name. */
	float              font_size;     /**< Font size in DIPs. */
	FluxColor          text_color;    /**< Text foreground color. */
	FluxTextAlign      text_alignment;
	FluxTextVAlign     text_vert_alignment;
	FluxFontWeight     font_weight;
	uint8_t            max_lines;
	bool               word_wrap;

	char const        *label;
	char const        *icon_name;
	FluxButtonStyle    button_style;
	bool               is_checked;

	FluxCheckState     check_state;

	float              min_value;
	float              max_value;
	float              current_value;
	float              step;

	/* Slider-only: thumb tracking + tick marks (WinUI IntermediateValue / TickBar). */
	float              slider_intermediate;
	float              slider_tick_frequency;
	uint8_t            slider_tick_placement; /**< FluxTickPlacement value. */

	FluxScrollSnapshot scroll;                /**< ScrollViewer-only render state. */
	FluxEditSnapshot   edit;                  /**< Text-edit-only render state (TextBox/PasswordBox/NumberBox). */
	bool               indeterminate;

	uint8_t            nb_spin_placement; /**< FluxNBSpinPlacement value. */
	bool               nb_up_enabled;
	bool               nb_down_enabled;
	float              hover_local_x; /**< Cursor X within bounds, or -1 when not hovered. */
	float              hover_local_y; /**< Cursor Y within bounds. */

	/* NavigationView selection indicator (pane overlay), pane-local px. */
	float              nav_ind_top;
	float              nav_ind_bottom;
	float              nav_ind_opacity;
	float              nav_shadow_opacity; /**< Minimal overlay-pane drop-shadow strength (0=none). */
	bool               nav_top;            /**< Pane is the horizontal Top bar. */
	uint8_t            nav_item_kind;      /**< FluxNavItemKind of the strip/pane item. */
	uint8_t            nav_depth;          /**< Hierarchy depth (31px indent per level). */
	bool               nav_has_children;   /**< Show the expand chevron. */
	bool               nav_expanded;       /**< Chevron points up while expanded. */

	/* TabView strip item. */
	uint8_t            tab_kind;      /**< FluxTabKind of the strip item. */
	bool               tab_separator; /**< Draw the 1px right-edge divider. */
	bool               tab_closable;  /**< Close button may be shown (IsClosable). */
	bool               tab_compact;   /**< Icon-only (TabWidthMode::Compact, unselected). */
} FluxRenderSnapshot;

/** @brief Build an immutable render snapshot for one node. */
void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd);

#ifdef __cplusplus
}
#endif

#endif
