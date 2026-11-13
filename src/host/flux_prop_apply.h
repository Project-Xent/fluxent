#ifndef FLUX_PROP_APPLY_H
#define FLUX_PROP_APPLY_H

#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_node_store.h"

void flux_prop_button_label(FluxStore *store, XentNodeId id, char const *label);

void flux_prop_button_icon(FluxStore *store, XentNodeId id, char const *icon_name);

void flux_prop_button_style(FluxStore *store, XentNodeId id, FluxButtonStyle style);

void flux_prop_button_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_checkbox_label(FluxStore *store, XentNodeId id, char const *label);

void flux_prop_checkbox_state(FluxStore *store, XentNodeId id, FluxCheckState state);

void flux_prop_checkbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_slider_value(FluxStore *store, XentNodeId id, float value);

void flux_prop_slider_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

void flux_prop_slider_step(FluxStore *store, XentNodeId id, float step);

void flux_prop_slider_tick_frequency(FluxStore *store, XentNodeId id, float frequency);

void flux_prop_slider_tick_placement(FluxStore *store, XentNodeId id, FluxTickPlacement placement);

void flux_prop_slider_snaps_to(FluxStore *store, XentNodeId id, FluxSliderSnapsTo snaps_to);

void flux_prop_slider_small_change(FluxStore *store, XentNodeId id, float small_change);

void flux_prop_slider_large_change(FluxStore *store, XentNodeId id, float large_change);

void flux_prop_slider_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_text_content(FluxStore *store, XentNodeId id, char const *content);

void flux_prop_text_fontsize(FluxStore *store, XentNodeId id, float size);

void flux_prop_text_color(FluxStore *store, XentNodeId id, FluxColor color);

void flux_prop_text_alignment(FluxStore *store, XentNodeId id, FluxTextAlign align);

void flux_prop_text_weight(FluxStore *store, XentNodeId id, FluxFontWeight weight);

void flux_prop_text_maxlines(FluxStore *store, XentNodeId id, uint32_t max_lines);

void flux_prop_text_trim(FluxStore *store, XentNodeId id, XentTextTrim trim);

void flux_prop_text_wrap(FluxStore *store, XentNodeId id, bool wrap);

void flux_prop_text_selectable(FluxStore *store, XentNodeId id, bool selectable);

void flux_prop_textbox_content(FluxStore *store, XentNodeId id, char const *content);

void flux_prop_textbox_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);

void flux_prop_textbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_textbox_multiline(FluxStore *store, XentNodeId id, bool multiline);

void flux_prop_textbox_readonly(FluxStore *store, XentNodeId id, bool readonly);

void flux_prop_progress_value(FluxStore *store, XentNodeId id, float value);

void flux_prop_progress_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

void flux_prop_progress_indeterminate(FluxStore *store, XentNodeId id, bool indeterminate);

void flux_prop_scroll_offset(FluxStore *store, XentNodeId id, float x, float y);

void flux_prop_node_background(FluxStore *store, XentNodeId id, FluxColor color);

void flux_prop_node_border(FluxStore *store, XentNodeId id, FluxColor color, float width);

void flux_prop_radius(FluxStore *store, XentNodeId id, float radius);

void flux_prop_password_content(FluxStore *store, XentNodeId id, char const *content);

void flux_prop_password_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);

void flux_prop_password_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_numberbox_value(FluxStore *store, XentNodeId id, double value);

void flux_prop_numberbox_range(FluxStore *store, XentNodeId id, double min_val, double max_val);

void flux_prop_numberbox_step(FluxStore *store, XentNodeId id, double step);

void flux_prop_numberbox_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_numberbox_spin_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_hyperlink_label(FluxStore *store, XentNodeId id, char const *label);

void flux_prop_hyperlink_url(FluxStore *store, XentNodeId id, char const *url);

void flux_prop_hyperlink_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_repeat_label(FluxStore *store, XentNodeId id, char const *label);

void flux_prop_repeat_icon(FluxStore *store, XentNodeId id, char const *icon_name);

void flux_prop_repeat_style(FluxStore *store, XentNodeId id, FluxButtonStyle style);

void flux_prop_repeat_enabled(FluxStore *store, XentNodeId id, bool enabled);

void flux_prop_repeat_timing(FluxStore *store, XentNodeId id, uint32_t delay_ms, uint32_t interval_ms);

void flux_prop_progressring_value(FluxStore *store, XentNodeId id, float value);

void flux_prop_progressring_range(FluxStore *store, XentNodeId id, float min_val, float max_val);

void flux_prop_progressring_indeterminate(FluxStore *store, XentNodeId id, bool indeterminate);

void flux_prop_badge_mode(FluxStore *store, XentNodeId id, FluxInfoBadgeMode mode);

void flux_prop_badge_value(FluxStore *store, XentNodeId id, int32_t value);

void flux_prop_combo_selected(FluxStore *store, XentNodeId id, int index);

void flux_prop_combo_items(FluxStore *store, XentNodeId id, char const *const *items, int count);

void flux_prop_infobar_severity(FluxStore *store, XentNodeId id, FluxInfoBarSeverity severity);

void flux_prop_infobar_title(FluxStore *store, XentNodeId id, char const *title);

void flux_prop_infobar_message(FluxStore *store, XentNodeId id, char const *message);

void flux_prop_infobar_open(FluxStore *store, XentNodeId id, bool open);
void flux_prop_infobar_closable(FluxStore *store, XentNodeId id, bool closable);

void flux_prop_expander_expanded(FluxStore *store, XentNodeId id, XentCtx *ctx, bool expanded);
void flux_prop_expander_header(FluxStore *store, XentNodeId id, char const *header);
void flux_prop_expander_width(FluxStore *store, XentNodeId id, float width);
void flux_prop_expander_content_height(FluxStore *store, XentNodeId id, float height);

void flux_prop_image_source(FluxStore *store, XentNodeId id, char const *source);

void flux_prop_image_stretch(FluxStore *store, XentNodeId id, FluxImageStretch stretch);

void flux_prop_node_tooltip(FluxStore *store, XentNodeId id, char const *text);

#endif
