#ifndef FLUX_RENDER_SNAPSHOT_H
#define FLUX_RENDER_SNAPSHOT_H

#include <fluxent/flux_node_store.h>
#include "controls/behavior/flux_calendar.h"
#include "controls/behavior/flux_color_picker.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"
#include "store/flux_node_data.h"

typedef struct FluxEditSnapshot {
	uint32_t       cursor_position;
	uint32_t       selection_start, selection_end;
	float          scroll_offset_x;
	float          scroll_offset_y;
	wchar_t const *composition_text;
	uint32_t       composition_length, composition_cursor;
	FluxColor      selection_color;
	bool           readonly;
	bool           multiline;
} FluxEditSnapshot;

typedef struct FluxTextSnapshot {
	char const    *text_content;
	char const    *font_family;
	FluxColor      text_color;
	FluxFontWeight font_weight;
	FluxTextAlign  text_alignment;
	FluxTextVAlign text_vert_alignment;
	uint8_t        max_lines;
	XentTextTrim   trim;
	uint32_t       selection_start;
	uint32_t       selection_end;
	bool           word_wrap;
	bool           selectable;
} FluxTextSnapshot;

typedef struct FluxButtonSnapshot {
	char const     *label;
	char const     *text_content;
	char const     *icon_name;
	FluxColor       text_color;
	FluxButtonStyle button_style;
	bool            pressed_primary;
	bool            is_checked;
} FluxButtonSnapshot;

typedef struct FluxCheckSnapshot {
	char const    *label;
	FluxCheckState check_state;
} FluxCheckSnapshot;

typedef struct FluxSwitchSnapshot {
	char const    *label;
	FluxCheckState check_state;
} FluxSwitchSnapshot;

typedef struct FluxSliderSnapshot {
	float   min_value;
	float   max_value;
	float   current_value;
	float   step;
	float   slider_intermediate;
	float   slider_tick_frequency;
	uint8_t slider_tick_placement;
	bool    vertical;
	bool    color_picker;
	bool    is_range;
	bool    thumb_held;
	uint8_t active_thumb;
	float   range_value [2];
	float   range_live [2];
} FluxSliderSnapshot;

typedef struct FluxTextBoxSnapshot {
	char const      *text_content;
	char const      *placeholder;
	char const      *font_family;
	FluxColor        text_color;
	FluxEditSnapshot edit;
	bool             is_checked;
	uint8_t          nb_spin_placement;
	bool             nb_up_enabled;
	bool             nb_down_enabled;
	bool             combo_editor;
} FluxTextBoxSnapshot;

typedef struct FluxProgressSnapshot {
	float      min_value;
	float      max_value;
	float      current_value;
	bool       indeterminate;
	uint8_t    state;
	bool       inactive;
	XentInsets padding;
} FluxProgressSnapshot;

typedef struct FluxImageSnapshot {
	char const      *text_content;
	FluxImageStretch stretch;
} FluxImageSnapshot;

typedef struct FluxInfoBadgeSnapshot {
	char const       *icon_name;
	int32_t           value;
	FluxInfoBadgeMode mode;
} FluxInfoBadgeSnapshot;

typedef struct FluxInfoBarSnapshot {
	char const         *label;
	char const         *text_content;
	FluxInfoBarSeverity severity;
	bool                is_closable;
} FluxInfoBarSnapshot;

typedef struct FluxComboSnapshot {
	char const *text_content;
	char const *placeholder;
	bool        is_checked;
	bool        editable;
	bool        edit_active;
} FluxComboSnapshot;

typedef struct FluxScrollSnapshot {
	float            x, y;
	float            origin_x, origin_y;
	float            content_w, content_h;
	FluxScrollBarVis h_vis, v_vis;
	uint8_t          mouse_over;
	float            mouse_local_x, mouse_local_y;
	uint8_t          drag_axis;
	uint8_t          v_up_pressed, v_dn_pressed;
	uint8_t          h_lf_pressed, h_rg_pressed;
	double           last_activity_time;
} FluxScrollSnapshot;

typedef struct FluxScrollBarSnapshot {
	float   value, minimum, maximum, viewport;
	uint8_t orientation;
	uint8_t indicator_mode;
	uint8_t dragging;
	uint8_t dec_pressed, inc_pressed;
	uint8_t large_dec_pressed, large_inc_pressed;
} FluxScrollBarSnapshot;

typedef struct FluxLoopingSnapshot {
	uint8_t buttons_shown;
	uint8_t up_pressed;
	uint8_t down_pressed;
} FluxLoopingSnapshot;

typedef struct FluxLoopingItemSnapshot {
	char const *label;
	bool        selected;
	bool        accent;
	bool        left;
	float       band_y;
	float       band_h;
} FluxLoopingItemSnapshot;

typedef struct FluxTabSnapshot {
	char const *label;
	char const *icon_name;
	uint8_t     tab_kind;
	bool        is_checked;
	bool        tab_separator;
	bool        tab_closable;
	bool        tab_compact;
} FluxTabSnapshot;

typedef struct FluxNavSnapshot {
	char const *label;
	char const *icon_name;
	float       nav_ind_top;
	float       nav_ind_bottom;
	float       nav_ind_opacity;
	float       nav_ind_indent;
	float       nav_ind_clip_top;
	float       nav_ind_clip_bottom;
	float       nav_shadow_opacity;
	float       nav_content_x;
	float       nav_content_y;
	bool        nav_is_pane;
	bool        nav_pane_solid;
	bool        nav_top;
	uint8_t     nav_item_kind;
	uint8_t     nav_depth;
	bool        nav_has_children;
	bool        nav_expanded;
	bool        is_checked;
} FluxNavSnapshot;

typedef struct FluxMenuSnapshot {
	char const *label;
	bool        is_checked;
} FluxMenuSnapshot;

typedef struct FluxExpanderSnapshot {
	bool is_checked;
} FluxExpanderSnapshot;

typedef struct FluxSelectorItemSnapshot {
	char const *text;
	uint32_t    icon_glyph;
	bool        selected;
	float       pill_t;
} FluxSelectorItemSnapshot;

typedef struct FluxBreadcrumbSnapshot {
	char const *label;
	bool        is_ellipsis;
	bool        is_last;
	bool        show_chevron;
	float       content_w;
} FluxBreadcrumbSnapshot;

typedef struct FluxRatingSnapshot {
	double      value;
	double      hover_value;
	double      placeholder_value;
	int         max_rating;
	float       star_size;
	float       item_spacing;
	uint32_t    set_glyph;
	uint32_t    unset_glyph;
	char const *caption;
	bool        is_read_only;
} FluxRatingSnapshot;

typedef struct FluxListItemSnapshot {
	uint8_t kind;
	bool    is_selected;
	bool    multi;
} FluxListItemSnapshot;

typedef struct FluxTreeItemSnapshot {
	char const *label;
	uint32_t    glyph;
	int16_t     depth;
	uint8_t     sel_state;
	uint8_t     flags;
} FluxTreeItemSnapshot;

typedef struct FluxFlipSnapshot {
	bool    vertical;
	bool    prev_enabled;
	bool    next_enabled;
	bool    buttons_alive;
	uint8_t pressed_btn;
} FluxFlipSnapshot;

typedef struct FluxPipsSnapshot {
	int     count;
	int     selected;
	int     window_start;
	int     visible;
	bool    vertical;
	uint8_t nav_vis;
	int     pressed_pip;
	uint8_t pressed_nav;
} FluxPipsSnapshot;

typedef struct FluxRefreshSnapshot {
	int   state;
	int   direction;
	float interaction_ratio;
	float threshold_ratio;
	float visualizer_size;
	float glyph_angle;
	float glyph_opacity;
	float glyph_scale;
} FluxRefreshSnapshot;

typedef struct FluxPagerSnapshot {
	int         count;
	int         selected;
	uint8_t     display_mode;
	uint8_t     first_state;
	uint8_t     prev_state;
	uint8_t     next_state;
	uint8_t     last_state;
	int16_t     cells [12];
	uint8_t     cell_count;
	int16_t     pressed_elem;
	char const *prefix;
	char const *suffix;
} FluxPagerSnapshot;

typedef struct FluxSplitPaneSnapshot {
	bool    overlay;
	uint8_t placement;
	bool    divider;
} FluxSplitPaneSnapshot;

typedef struct FluxTitleBarSnapshot {
	char const *title;
	char const *subtitle;
	char const *icon_glyph;
	float       title_w;
	int         pressed;
	bool        show_back;
	bool        back_disabled;
	bool        show_pane_toggle;
	bool        caption_buttons;
	bool        maximized;
} FluxTitleBarSnapshot;

typedef struct FluxPersonSnapshot {
	char const *initials;
	char const *image_path;
	char const *badge_glyph;
	int         badge_number;
	float       diameter;
	bool        is_group;
} FluxPersonSnapshot;

typedef struct FluxRenderSnap {
	uint64_t        id;
	FluxControlType type;

	FluxColor       background;
	FluxColor       border_color;
	float           corner_radius;
	float           border_width;
	float           opacity;
	float           font_size;

	float           hover_local_x;
	float           hover_local_y;

	union
	{
		FluxTextSnapshot         text;
		FluxButtonSnapshot       button;
		FluxCheckSnapshot        check;
		FluxSwitchSnapshot       sw;
		FluxSliderSnapshot       slider;
		FluxTextBoxSnapshot      textbox;
		FluxProgressSnapshot     progress;
		FluxImageSnapshot        image;
		FluxInfoBadgeSnapshot    info_badge;
		FluxInfoBarSnapshot      info_bar;
		FluxComboSnapshot        combo;
		FluxScrollSnapshot       scroll;
		FluxScrollBarSnapshot    scroll_bar;
		FluxLoopingSnapshot      looping;
		FluxLoopingItemSnapshot  loop_item;
		FluxCalendarSnapshot     calendar;
		FluxColorPickerSnapshot  color_picker;
		FluxPickerSnapshot       picker;
		FluxPickerCardSnapshot   picker_card;
		FluxTabSnapshot          tab;
		FluxNavSnapshot          nav;
		FluxMenuSnapshot         menu;
		FluxExpanderSnapshot     expander;
		FluxRatingSnapshot       rating;
		FluxSelectorItemSnapshot selector;
		FluxBreadcrumbSnapshot   breadcrumb;
		FluxListItemSnapshot     list_item;
		FluxTreeItemSnapshot     tree_item;
		FluxFlipSnapshot         flip;
		FluxPipsSnapshot         pips;
		FluxRefreshSnapshot      refresh;
		FluxPersonSnapshot       person;
		FluxPagerSnapshot        pager;
		FluxSplitPaneSnapshot    split_pane;
		FluxTitleBarSnapshot     title_bar;
	} u;
} FluxRenderSnap;

void flux_snapshot_build(FluxRenderSnap *snap, XentCtx const *ctx, XentNodeId node, FluxNodeData const *nd);

#endif
