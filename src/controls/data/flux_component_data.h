/**
 * @file flux_component_data.h
 * @brief Aggregate header for all control-specific data structures.
 *
 * This header pulls in per-control data headers for convenience.
 * New code should prefer including only the specific headers needed.
 *
 * ## Per-control headers:
 * - controls/flux_text_data.h      — FluxTextData, FluxTextAlign, FluxFontWeight
 * - controls/flux_button_data.h    — FluxButtonData, FluxRepeatButtonData, FluxHyperlinkData
 * - controls/flux_slider_data.h    — FluxSliderData
 * - controls/flux_checkbox_data.h  — FluxCheckboxData, FluxCheckState
 * - controls/flux_scroll_data.h    — FluxScrollData, FluxScrollBarVis
 * - controls/flux_textbox_data.h   — FluxTextBoxData, FluxPasswordBoxData, FluxNumberBoxData
 * - controls/flux_progress_data.h  — FluxProgressData, FluxProgressRingData
 * - controls/flux_image_data.h     — FluxImageData
 * - controls/flux_info_badge_data.h — FluxInfoBadgeData, FluxInfoBadgeMode
 * - controls/flux_info_bar_data.h  — FluxInfoBarData, FluxInfoBarSeverity
 * - controls/flux_combo_box_data.h — FluxComboBoxData
 * - controls/flux_content_dialog_data.h — FluxDialogResult
 * - controls/flux_menu_bar_data.h  — FluxMenuBarData, FluxMenuBarItem
 * - controls/flux_nav_view_data.h  — FluxNavViewData, FluxNavViewItem
 * - controls/flux_tab_view_data.h  — FluxTabViewData, FluxTabViewItem
 *
 * RadioButton and ToggleSwitch deliberately share FluxCheckboxData (the toggle
 * family — label + FluxCheckState); Host materialize / private factory create them.
 *
 * BreadcrumbBar / Expander / FlipView / PipsPager / Refresh / TreeView keep no
 * public data header: their retained runtimes live in the private
 * src/controls/behavior/*_internal.h next to the behavior that owns them.
 */
#ifndef FLUX_COMPONENT_DATA_H
#define FLUX_COMPONENT_DATA_H

#include "flux_text_data.h"
#include "store/flux_node_data.h"
#include "flux_button_data.h"
#include "flux_slider_data.h"
#include "flux_checkbox_data.h"
#include "flux_scroll_data.h"
#include "flux_textbox_data.h"
#include "flux_progress_data.h"
#include "flux_image_data.h"
#include "flux_info_badge_data.h"
#include "flux_info_bar_data.h"
#include "flux_combo_box_data.h"
#include "flux_content_dialog_data.h"
#include "flux_menu_bar_data.h"
#include "flux_nav_view_data.h"
#include "flux_tab_view_data.h"
#include "flux_list_view_data.h"
#include "flux_items_view_data.h"
#include "flux_radio_buttons_data.h"
#include "flux_rating_data.h"
#include "flux_teaching_tip_data.h"
#include "flux_selector_bar_data.h"
#include "flux_person_picture_data.h"
#include "flux_pager_data.h"
#include "flux_split_view_data.h"
#include "flux_title_bar_data.h"

#endif
