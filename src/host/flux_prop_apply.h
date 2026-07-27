/**
 * @file flux_prop_apply.h
 * @brief Host-private control property apply helpers.
 *
 * Single mutation surface for control fields. Host SET_PROP, view sync,
 * and control self-updates share these helpers. No public flux_*_set_*.
 */
#ifndef FLUX_PROP_APPLY_H
#define FLUX_PROP_APPLY_H

#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_node_store.h"

/** @brief Set the button's text label. */
void flux_prop_button_label(FluxStore *store, XentNodeId id, char const *label);

/** @brief Set the button's icon by name (looked up in flux_icon.h table). */
void flux_prop_button_icon(FluxStore *store, XentNodeId id, char const *icon_name);

/** @brief Set the button's visual style (default, accent, subtle, text). */
void flux_prop_button_style(FluxStore *store, XentNodeId id, FluxButtonStyle style);

/** @brief Enable or disable the button. Disabled buttons ignore input. */
void flux_prop_button_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the checkbox's text label. */
void flux_prop_checkbox_label(FluxStore *store, XentNodeId id, char const *label);

/** @brief Set the checkbox's check state (unchecked, checked, indeterminate). */
void flux_prop_checkbox_state(FluxStore *store, XentNodeId id, FluxCheckState state);

/** @brief Enable or disable the checkbox. */
void flux_prop_checkbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the slider's current value. Clamped to [min, max]. */
void flux_prop_slider_value(FluxStore *store, XentNodeId id, float value);

/** @brief Set the slider's minimum and maximum values. */
void flux_prop_slider_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

/** @brief Set the slider's snap grid (WinUI StepFrequency; default 1, <=0 disables snapping). */
void flux_prop_slider_step(FluxStore *store, XentNodeId id, float step);

/** @brief Set the tick mark interval (WinUI TickFrequency; 0 = no ticks). */
void flux_prop_slider_tick_frequency(FluxStore *store, XentNodeId id, float frequency);

/** @brief Set where tick marks render (WinUI TickPlacement). */
void flux_prop_slider_tick_placement(FluxStore *store, XentNodeId id, FluxTickPlacement placement);

/** @brief Choose the drag snap grid: step multiples or tick multiples (WinUI SnapsTo). */
void flux_prop_slider_snaps_to(FluxStore *store, XentNodeId id, FluxSliderSnapsTo snaps_to);

/** @brief Set the arrow-key delta (WinUI SmallChange; default 1). */
void flux_prop_slider_small_change(FluxStore *store, XentNodeId id, float small_change);

/** @brief Set the PageUp/PageDown delta (WinUI LargeChange; default 10). */
void flux_prop_slider_large_change(FluxStore *store, XentNodeId id, float large_change);

/** @brief Enable or disable the slider. */
void flux_prop_slider_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the text content (UTF-8). */
void flux_prop_text_content(FluxStore *store, XentNodeId id, char const *content);

/** @brief Set the font size in DIPs. */
void flux_prop_text_fontsize(FluxStore *store, XentNodeId id, float size);

/** @brief Set the text foreground color. */
void flux_prop_text_color(FluxStore *store, XentNodeId id, FluxColor color);

/** @brief Set the horizontal text alignment. */
void flux_prop_text_alignment(FluxStore *store, XentNodeId id, FluxTextAlign align);

/** @brief Set the font weight (regular, semi-bold, bold). */
void flux_prop_text_weight(FluxStore *store, XentNodeId id, FluxFontWeight weight);

/** @brief Set the text content (UTF-8). Replaces existing text. */
void flux_prop_textbox_content(FluxStore *store, XentNodeId id, char const *content);

/** @brief Set the placeholder text shown when empty. */
void flux_prop_textbox_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);

/** @brief Enable or disable the text box. */
void flux_prop_textbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the current progress value. */
void flux_prop_progress_value(FluxStore *store, XentNodeId id, float value);

/** @brief Set the progress range. */
void flux_prop_progress_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

/** @brief Set the maximum progress value (determines percentage). */
void flux_prop_progress_max(FluxStore *store, XentNodeId id, float max_val);

/** @brief Enable indeterminate mode (animated, no specific progress). */
void flux_prop_progress_indeterminate(FluxStore *store, XentNodeId id, bool indeterminate);

/** @brief Set the scroll offset (x, y) in DIPs. */
void flux_prop_scroll_offset(FluxStore *store, XentNodeId id, float x, float y);

/** @brief Set the node's background fill color. */
void flux_prop_node_background(FluxStore *store, XentNodeId id, FluxColor color);

/** @brief Set the node's border color and stroke width. */
void flux_prop_node_border(FluxStore *store, XentNodeId id, FluxColor color, float width);

/** @brief Set the corner radius for rounded rectangles. */
void flux_prop_radius(FluxStore *store, XentNodeId id, float radius);

/** @brief Set the overall opacity [0.0, 1.0]. */
void flux_prop_node_opacity(FluxStore *store, XentNodeId id, float opacity);

/** @brief Set the password content (UTF-8). */
void flux_prop_password_content(FluxStore *store, XentNodeId id, char const *content);

/** @brief Set the placeholder text shown when empty. */
void flux_prop_password_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);

/** @brief Enable or disable the password box. */
void flux_prop_password_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Show or hide the password in plain text (reveal mode). */
void flux_prop_password_reveal(FluxStore *store, XentNodeId id, bool show_plain);

/** @brief Set the numeric value. */
void flux_prop_numberbox_value(FluxStore *store, XentNodeId id, double value);

/** @brief Set the allowed value range [min, max]. */
void flux_prop_numberbox_range(FluxStore *store, XentNodeId id, double min_val, double max_val);

/** @brief Set the increment/decrement step size. */
void flux_prop_numberbox_step(FluxStore *store, XentNodeId id, double step);

/** @brief Enable or disable the number box. */
void flux_prop_numberbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Show or hide the spin buttons (+/-). */
void flux_prop_numberbox_spin_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the hyperlink's display text. */
void flux_prop_hyperlink_label(FluxStore *store, XentNodeId id, char const *label);

/** @brief Set the URL to open when clicked. */
void flux_prop_hyperlink_url(FluxStore *store, XentNodeId id, char const *url);

/** @brief Enable or disable the hyperlink. */
void flux_prop_hyperlink_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Set the button's text label. */
void flux_prop_repeat_label(FluxStore *store, XentNodeId id, char const *label);

/** @brief Set the button's icon by name. */
void flux_prop_repeat_icon(FluxStore *store, XentNodeId id, char const *icon_name);

/** @brief Set the button's visual style. */
void flux_prop_repeat_style(FluxStore *store, XentNodeId id, FluxButtonStyle style);

/** @brief Enable or disable the button. */
void flux_prop_repeat_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_repeat_timing(FluxStore *store, XentNodeId id, uint32_t delay_ms, uint32_t interval_ms);

/** @brief Set the current progress value. */
void flux_prop_progressring_value(FluxStore *store, XentNodeId id, float value);

/** @brief Set the progress range. */
void flux_prop_progressring_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

/** @brief Set the maximum progress value. */
void flux_prop_progressring_max(FluxStore *store, XentNodeId id, float max_val);

/** @brief Enable indeterminate mode (spinning animation). */
void flux_prop_progressring_indeterminate(FluxStore *store, XentNodeId id, bool indeterminate);

/** @brief Set the badge display mode (dot, icon, or value). */
void flux_prop_badge_mode(FluxStore *store, XentNodeId id, FluxInfoBadgeMode mode);

/** @brief Set the numeric value (for value mode). */
void flux_prop_badge_value(FluxStore *store, XentNodeId id, int32_t value);

/** @brief Set the icon name (for icon mode). */
void flux_prop_badge_icon(FluxStore *store, XentNodeId id, char const *icon_name);

/** @brief Set the combo box selection (-1 clears). Does not fire on_select. */
void flux_prop_combo_selected(FluxStore *store, XentNodeId id, int index);

/** @brief Replace the combo box item list (deep-copied). Selection is clamped. */
void flux_prop_combo_items(FluxStore *store, XentNodeId id, char const *const *items, int count);

/** @brief Set the info bar severity (icon glyph + color scheme). */
void flux_prop_infobar_severity(FluxStore *store, XentNodeId id, FluxInfoBarSeverity severity);

/** @brief Set the info bar title (bold leading text). */
void flux_prop_infobar_title(FluxStore *store, XentNodeId id, char const *title);

/** @brief Set the info bar message (body text). */
void flux_prop_infobar_message(FluxStore *store, XentNodeId id, char const *message);

/** @brief Show or hide the info bar. */
void flux_prop_infobar_open(FluxStore *store, XentNodeId id, bool open);
void flux_prop_infobar_closable(FluxStore *store, XentNodeId id, bool closable);

/** @brief Expand or collapse the expander (attaches/detaches its content). */
void flux_prop_expander_expanded(FluxStore *store, XentNodeId id, XentCtx *ctx, bool expanded);
void flux_prop_expander_header(FluxStore *store, XentNodeId id, char const *header);
void flux_prop_expander_width(FluxStore *store, XentNodeId id, float width);
void flux_prop_expander_content_height(FluxStore *store, XentNodeId id, float height);

/** @brief Set the image source path / URI. */
void flux_prop_image_source(FluxStore *store, XentNodeId id, char const *source);

/** @brief Set the image stretch mode. */
void flux_prop_image_stretch(FluxStore *store, XentNodeId id, FluxImageStretch stretch);

/** @brief Attach a tooltip to any node. Shown on hover after a delay. */
void flux_prop_node_tooltip(FluxStore *store, XentNodeId id, char const *text);

#endif
