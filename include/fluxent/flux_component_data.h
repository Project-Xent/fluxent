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
 */
#ifndef FLUX_COMPONENT_DATA_H
#define FLUX_COMPONENT_DATA_H

#include "controls/flux_text_data.h"
#include "controls/flux_button_data.h"
#include "controls/flux_slider_data.h"
#include "controls/flux_checkbox_data.h"
#include "controls/flux_scroll_data.h"
#include "controls/flux_textbox_data.h"
#include "controls/flux_progress_data.h"
#include "controls/flux_image_data.h"
#include "controls/flux_info_badge_data.h"

#endif /* FLUX_COMPONENT_DATA_H */
