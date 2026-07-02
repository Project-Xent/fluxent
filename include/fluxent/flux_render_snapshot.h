/**
 * @file flux_render_snapshot.h
 * @brief Immutable render command containing all visual state for a control.
 *
 * FluxRenderSnapshot captures a point-in-time snapshot of a control's
 * appearance and state. The engine collects these during tree traversal
 * and dispatches them to renderers during the execute phase.
 *
 * The snapshot is split into a small shared BASE (geometry, colors, opacity,
 * font size, hover position — fields every control draws) and a per-control
 * payload union @c u tagged by @c type. Each control's builder writes exactly
 * one union arm and its renderer reads exactly that arm; control-specific data
 * never bloats unrelated snapshots (Doctrine #1/#2, data dominates).
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
 * @brief Text-edit-private render state (TextBox / PasswordBox / NumberBox).
 *
 * Carries caret, selection, and IME composition for the text-input renderers.
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

/** @brief TextBlock payload — read only by flux_draw_text. */
typedef struct FluxTextSnapshot {
	char const    *text_content;        /**< Text to display (may be NULL). */
	char const    *font_family;         /**< Font family name. */
	FluxColor      text_color;          /**< Text foreground. */
	FluxFontWeight font_weight;         /**< Glyph weight. */
	FluxTextAlign  text_alignment;      /**< Horizontal alignment. */
	FluxTextVAlign text_vert_alignment; /**< Vertical alignment. */
	uint8_t        max_lines;           /**< Line clamp (0 = unlimited). */
	bool           word_wrap;           /**< Wrap on word boundaries. */
} FluxTextSnapshot;

/** @brief Button-family payload (Button/Toggle/Dropdown/Split/Hyperlink/RepeatButton). */
typedef struct FluxButtonSnapshot {
	char const     *label;        /**< Button caption (preferred). */
	char const     *text_content; /**< Fallback caption when label is NULL. */
	char const     *icon_name;    /**< Leading icon glyph name, or NULL. */
	FluxColor       text_color;   /**< Label foreground. */
	FluxButtonStyle button_style; /**< Visual style (standard/accent/text...). */
	bool            is_checked;   /**< Toggle/menu open/checked state. */
} FluxButtonSnapshot;

/** @brief Checkbox / RadioButton payload. */
typedef struct FluxCheckSnapshot {
	char const    *label;       /**< Caption text (may be NULL). */
	FluxCheckState check_state; /**< Unchecked / checked / indeterminate. */
} FluxCheckSnapshot;

/** @brief ToggleSwitch payload. */
typedef struct FluxSwitchSnapshot {
	char const    *label;       /**< Caption text (may be NULL). */
	FluxCheckState check_state; /**< On (CHECKED) / off (UNCHECKED). */
} FluxSwitchSnapshot;

/** @brief Slider payload — track range, thumb tracking, and tick marks. */
typedef struct FluxSliderSnapshot {
	float   min_value;             /**< Track minimum. */
	float   max_value;             /**< Track maximum. */
	float   current_value;         /**< Committed value. */
	float   step;                  /**< Step frequency. */
	float   slider_intermediate;   /**< Live drag value (WinUI IntermediateValue). */
	float   slider_tick_frequency; /**< Tick spacing (TickBar). */
	uint8_t slider_tick_placement; /**< FluxTickPlacement value. */
} FluxSliderSnapshot;

/** @brief Text-input payload (TextBox / PasswordBox / NumberBox). */
typedef struct FluxTextBoxSnapshot {
	char const      *text_content;      /**< Current text (may be NULL). */
	char const      *placeholder;       /**< Placeholder text (may be NULL). */
	char const      *font_family;       /**< Font family name. */
	FluxColor        text_color;        /**< Text foreground. */
	FluxEditSnapshot edit;              /**< Caret / selection / IME state. */
	bool             is_checked;        /**< PasswordBox: reveal plain text. */
	uint8_t          nb_spin_placement; /**< NumberBox: FluxNBSpinPlacement value. */
	bool             nb_up_enabled;     /**< NumberBox: increment enabled. */
	bool             nb_down_enabled;   /**< NumberBox: decrement enabled. */
} FluxTextBoxSnapshot;

/** @brief Progress bar / ring payload. */
typedef struct FluxProgressSnapshot {
	float min_value;     /**< Range minimum (0% mark). */
	float max_value;     /**< Range maximum (100% mark). */
	float current_value; /**< Current progress value. */
	bool  indeterminate; /**< Looping animation instead of a percentage. */
} FluxProgressSnapshot;

/** @brief Image payload. */
typedef struct FluxImageSnapshot {
	char const      *text_content; /**< Source path or URI. */
	FluxImageStretch stretch;      /**< Scaling mode. */
} FluxImageSnapshot;

/** @brief InfoBadge payload. */
typedef struct FluxInfoBadgeSnapshot {
	char const       *icon_name; /**< Icon glyph name (ICON mode). */
	int32_t           value;     /**< Numeric value (NUMBER mode). */
	FluxInfoBadgeMode mode;      /**< Dot / number / icon presentation. */
} FluxInfoBadgeSnapshot;

/** @brief InfoBar payload. */
typedef struct FluxInfoBarSnapshot {
	char const         *label;        /**< Bold leading title (may be NULL). */
	char const         *text_content; /**< Body message (may be NULL). */
	FluxInfoBarSeverity severity;     /**< Selects icon glyph + palette. */
	bool                is_closable;  /**< Show the trailing close button. */
} FluxInfoBarSnapshot;

/** @brief ComboBox closed-box payload. */
typedef struct FluxComboSnapshot {
	char const *text_content; /**< Selected item text, or NULL. */
	char const *placeholder;  /**< Placeholder when nothing is selected. */
	bool        is_checked;   /**< Drop-down is open. */
} FluxComboSnapshot;

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

/** @brief TabView strip-item payload. */
typedef struct FluxTabSnapshot {
	char const *label;         /**< Tab caption (may be NULL when compact). */
	char const *icon_name;     /**< Icon glyph name, or NULL. */
	uint8_t     tab_kind;      /**< FluxTabKind of the strip item. */
	bool        is_checked;    /**< Tab is selected. */
	bool        tab_separator; /**< Draw the 1px right-edge divider. */
	bool        tab_closable;  /**< Close button may be shown (IsClosable). */
	bool        tab_compact;   /**< Icon-only (TabWidthMode::Compact, unselected). */
} FluxTabSnapshot;

/** @brief NavigationView pane + item payload (selection indicator overlay). */
typedef struct FluxNavSnapshot {
	char const *label;              /**< Item label, or NULL when hidden. */
	char const *icon_name;          /**< Item icon glyph name, or NULL. */
	float       nav_ind_top;        /**< Selection pill top (pane-local px). */
	float       nav_ind_bottom;     /**< Selection pill bottom (pane-local px). */
	float       nav_ind_opacity;    /**< Selection pill opacity. */
	float       nav_shadow_opacity; /**< Overlay-pane drop-shadow strength (0=none). */
	bool        nav_top;            /**< Pane is the horizontal Top bar. */
	uint8_t     nav_item_kind;      /**< FluxNavItemKind of the strip/pane item. */
	uint8_t     nav_depth;          /**< Hierarchy depth (31px indent per level). */
	bool        nav_has_children;   /**< Show the expand chevron. */
	bool        nav_expanded;       /**< Chevron points up while expanded. */
	bool        is_checked;         /**< Item is the selected entry. */
} FluxNavSnapshot;

/** @brief MenuBar item payload. */
typedef struct FluxMenuSnapshot {
	char const *label;      /**< Menu title. */
	bool        is_checked; /**< Menu is open. */
} FluxMenuSnapshot;

/** @brief Expander header payload. */
typedef struct FluxExpanderSnapshot {
	bool is_checked; /**< Expander is expanded. */
} FluxExpanderSnapshot;

/** @brief ListView-family cell payload. */
typedef struct FluxListItemSnapshot {
	uint8_t kind;        /**< XtkListKind — selects the chrome (list/list box/grid). */
	bool    is_selected; /**< Cell is in the selection set (resolved per mode). */
	bool    multi;       /**< Multiple mode: draw the checkbox. */
} FluxListItemSnapshot;

/**
 * @brief Immutable snapshot of a control's visual state.
 *
 * The leading fields are the shared BASE; @c u carries the control-specific
 * payload selected by @c type.
 */
typedef struct FluxRenderSnapshot {
	uint64_t        id;            /**< Node ID. */
	FluxControlType type;          /**< Control type (tags @c u). */

	FluxColor       background;    /**< Fill color. */
	FluxColor       border_color;  /**< Border stroke color. */
	float           corner_radius; /**< Corner radius in DIPs. */
	float           border_width;  /**< Border stroke width. */
	float           opacity;       /**< Overall opacity (0..1). */
	float           font_size;     /**< Font size in DIPs (shared text metric). */

	float           hover_local_x; /**< Cursor X within bounds, or -1 when not hovered. */
	float           hover_local_y; /**< Cursor Y within bounds. */

	union {
		FluxTextSnapshot      text;       /**< FLUX_CONTROL_TEXT. */
		FluxButtonSnapshot    button;     /**< Button family. */
		FluxCheckSnapshot     check;      /**< Checkbox / Radio. */
		FluxSwitchSnapshot    sw;         /**< ToggleSwitch. */
		FluxSliderSnapshot    slider;     /**< Slider. */
		FluxTextBoxSnapshot   textbox;    /**< TextBox / PasswordBox / NumberBox. */
		FluxProgressSnapshot  progress;   /**< Progress bar / ring. */
		FluxImageSnapshot     image;      /**< Image. */
		FluxInfoBadgeSnapshot info_badge; /**< InfoBadge. */
		FluxInfoBarSnapshot   info_bar;   /**< InfoBar. */
		FluxComboSnapshot     combo;      /**< ComboBox. */
		FluxScrollSnapshot    scroll;     /**< ScrollViewer. */
		FluxTabSnapshot       tab;        /**< TabView item. */
		FluxNavSnapshot       nav;        /**< NavigationView pane / item. */
		FluxMenuSnapshot      menu;       /**< MenuBar item. */
		FluxExpanderSnapshot  expander;   /**< Expander header. */
		FluxListItemSnapshot  list_item;  /**< ListView row. */
	} u;
} FluxRenderSnapshot;

/** @brief Build an immutable render snapshot for one node. */
void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd);

#ifdef __cplusplus
}
#endif

#endif
