/**
 * @file flux_controls.h
 * @brief Control property setters for Fluxent UI components.
 *
 * This header provides functions to modify control properties after creation.
 * All setters follow a consistent pattern: they take a FluxNodeStore, a node ID,
 * and the new property value(s).
 *
 * @note Property changes take effect on the next frame. The node must exist
 *       in the store and have the appropriate component_data attached.
 */
#ifndef FLUX_CONTROLS_H
#define FLUX_CONTROLS_H

#include "flux_component_data.h"
#include "flux_node_store.h"
#include "flux_popup.h"

typedef struct FluxFlyout     FluxFlyout;
typedef struct FluxMenuFlyout FluxMenuFlyout;
typedef struct FluxWindow     FluxWindow;

/** @brief Full flyout binding parameters. */
typedef struct FluxFlyoutBindingInfo {
	FluxNodeStore *store;
	XentNodeId     id;
	FluxFlyout    *flyout;
	FluxPlacement  placement;
	XentContext   *xctx;
	FluxWindow    *window;
} FluxFlyoutBindingInfo;

/** @brief Full context-menu flyout binding parameters. */
typedef struct FluxContextFlyoutBindingInfo {
	FluxNodeStore  *store;
	XentNodeId      id;
	FluxMenuFlyout *menu;
	XentContext    *xctx;
	FluxWindow     *window;
} FluxContextFlyoutBindingInfo;

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Set the button's text label. */
void flux_button_set_label(FluxNodeStore *store, XentNodeId id, char const *label);

/** @brief Set the button's icon by name (looked up in flux_icon.h table). */
void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name);

/** @brief Set the button's visual style (default, accent, subtle, text). */
void flux_button_set_style(FluxNodeStore *store, XentNodeId id, FluxButtonStyle style);

/** @brief Enable or disable the button. Disabled buttons ignore input. */
void flux_button_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the checkbox's text label. */
void flux_checkbox_set_label(FluxNodeStore *store, XentNodeId id, char const *label);

/** @brief Set the checkbox's check state (unchecked, checked, indeterminate). */
void flux_checkbox_set_state(FluxNodeStore *store, XentNodeId id, FluxCheckState state);

/** @brief Enable or disable the checkbox. */
void flux_checkbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the slider's current value. Clamped to [min, max]. */
void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value);

/** @brief Set the slider's minimum and maximum values. */
void flux_slider_set_range(FluxNodeStore *store, XentNodeId id, float min_val, float max_val);

/** @brief Enable or disable the slider. */
void flux_slider_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the text content (UTF-8). */
void flux_text_set_content(FluxNodeStore *store, XentNodeId id, char const *content);

/** @brief Set the font size in DIPs. */
void flux_text_set_font_size(FluxNodeStore *store, XentNodeId id, float size);

/** @brief Set the text foreground color. */
void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color);

/** @brief Set the horizontal text alignment. */
void flux_text_set_alignment(FluxNodeStore *store, XentNodeId id, FluxTextAlign align);

/** @brief Set the font weight (regular, semi-bold, bold). */
void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight);

/** @brief Set the text content (UTF-8). Replaces existing text. */
void flux_textbox_set_content(FluxNodeStore *store, XentNodeId id, char const *content);

/** @brief Set the placeholder text shown when empty. */
void flux_textbox_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder);

/** @brief Enable or disable the text box. */
void flux_textbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the current progress value. */
void flux_progress_set_value(FluxNodeStore *store, XentNodeId id, float value);

/** @brief Set the maximum progress value (determines percentage). */
void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val);

/** @brief Enable indeterminate mode (animated, no specific progress). */
void flux_progress_set_indeterminate(FluxNodeStore *store, XentNodeId id, bool indeterminate);

/** @brief Set the scroll offset (x, y) in DIPs. */
void flux_scroll_set_offset(FluxNodeStore *store, XentNodeId id, float x, float y);

/** @brief Set the node's background fill color. */
void flux_node_set_background(FluxNodeStore *store, XentNodeId id, FluxColor color);

/** @brief Set the node's border color and stroke width. */
void flux_node_set_border(FluxNodeStore *store, XentNodeId id, FluxColor color, float width);

/** @brief Set the corner radius for rounded rectangles. */
void flux_node_set_corner_radius(FluxNodeStore *store, XentNodeId id, float radius);

/** @brief Set the overall opacity [0.0, 1.0]. */
void flux_node_set_opacity(FluxNodeStore *store, XentNodeId id, float opacity);

/** @brief Set the password content (UTF-8). */
void flux_password_set_content(FluxNodeStore *store, XentNodeId id, char const *content);

/** @brief Set the placeholder text shown when empty. */
void flux_password_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder);

/** @brief Enable or disable the password box. */
void flux_password_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Show or hide the password in plain text (reveal mode). */
void flux_password_set_reveal(FluxNodeStore *store, XentNodeId id, bool show_plain);

/** @brief Set the numeric value. */
void flux_numberbox_set_value(FluxNodeStore *store, XentNodeId id, double value);

/** @brief Set the allowed value range [min, max]. */
void flux_numberbox_set_range(FluxNodeStore *store, XentNodeId id, double min_val, double max_val);

/** @brief Set the increment/decrement step size. */
void flux_numberbox_set_step(FluxNodeStore *store, XentNodeId id, double step);

/** @brief Enable or disable the number box. */
void flux_numberbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Show or hide the spin buttons (+/-). */
void flux_numberbox_set_spin_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the hyperlink's display text. */
void flux_hyperlink_set_label(FluxNodeStore *store, XentNodeId id, char const *label);

/** @brief Set the URL to open when clicked. */
void flux_hyperlink_set_url(FluxNodeStore *store, XentNodeId id, char const *url);

/** @brief Enable or disable the hyperlink. */
void flux_hyperlink_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/** @brief Set the button's text label. */
void flux_repeat_button_set_label(FluxNodeStore *store, XentNodeId id, char const *label);

/** @brief Set the button's icon by name. */
void flux_repeat_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name);

/** @brief Set the button's visual style. */
void flux_repeat_button_set_style(FluxNodeStore *store, XentNodeId id, FluxButtonStyle style);

/** @brief Enable or disable the button. */
void flux_repeat_button_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/**
 * @brief Set the repeat timing parameters.
 * @param delay_ms    Initial delay before repeat starts (milliseconds).
 * @param interval_ms Interval between repeats (milliseconds).
 */
void flux_repeat_button_set_timing(FluxNodeStore *store, XentNodeId id, uint32_t delay_ms, uint32_t interval_ms);

/** @brief Set the current progress value. */
void flux_progress_ring_set_value(FluxNodeStore *store, XentNodeId id, float value);

/** @brief Set the maximum progress value. */
void flux_progress_ring_set_max(FluxNodeStore *store, XentNodeId id, float max_val);

/** @brief Enable indeterminate mode (spinning animation). */
void flux_progress_ring_set_indeterminate(FluxNodeStore *store, XentNodeId id, bool indeterminate);

/** @brief Set the badge display mode (dot, icon, or value). */
void flux_info_badge_set_mode(FluxNodeStore *store, XentNodeId id, FluxInfoBadgeMode mode);

/** @brief Set the numeric value (for value mode). */
void flux_info_badge_set_value(FluxNodeStore *store, XentNodeId id, int32_t value);

/** @brief Set the icon name (for icon mode). */
void flux_info_badge_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name);

/** @brief Attach a tooltip to any node. Shown on hover after a delay. */
void flux_node_set_tooltip(FluxNodeStore *store, XentNodeId id, char const *text);

/**
 * @brief Attach a flyout to a node's click handler.
 * @param flyout    The flyout to show (not owned).
 * @param placement Preferred placement relative to the node.
 * @see flux_flyout.h for the full FluxFlyout API.
 */
void flux_node_set_flyout(FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement);

/**
 * @brief Attach a flyout with explicit context references.
 * @param xctx   Layout context for anchor position lookup.
 * @param window Window for screen coordinate conversion.
 */
void flux_node_set_flyout_ex(FluxFlyoutBindingInfo const *info);

/** @brief Attach a context menu shown on right-click / long-press. */
void flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu);

/** @brief Attach a context menu with explicit context references. */
void flux_node_set_context_flyout_ex(FluxContextFlyoutBindingInfo const *info);

#ifdef __cplusplus
}
#endif

#endif
