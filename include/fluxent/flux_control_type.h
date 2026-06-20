/**
 * @file flux_control_type.h
 * @brief Control type enumeration for Fluxent UI controls.
 *
 * Defines control type identifiers used by the rendering pipeline, input
 * system, and control factory. Values are stored in xent-core's generic
 * node tag via xent_set_node_tag / xent_get_node_tag.
 */
#ifndef FLUX_CONTROL_TYPE_H
#define FLUX_CONTROL_TYPE_H

#include <stdint.h>
#include <xent/xent.h>
#include <xtk/xtk_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef XtkControlType FluxControlType;

#define FLUX_CONTROL_CONTAINER       XTK_CONTROL_CONTAINER
#define FLUX_CONTROL_TEXT            XTK_CONTROL_TEXT
#define FLUX_CONTROL_BUTTON          XTK_CONTROL_BUTTON
#define FLUX_CONTROL_TOGGLE_BUTTON   XTK_CONTROL_TOGGLE_BUTTON
#define FLUX_CONTROL_CHECKBOX        XTK_CONTROL_CHECKBOX
#define FLUX_CONTROL_RADIO           XTK_CONTROL_RADIO
#define FLUX_CONTROL_SWITCH          XTK_CONTROL_SWITCH
#define FLUX_CONTROL_SLIDER          XTK_CONTROL_SLIDER
#define FLUX_CONTROL_TEXT_INPUT       XTK_CONTROL_TEXT_INPUT
#define FLUX_CONTROL_SCROLL          XTK_CONTROL_SCROLL
#define FLUX_CONTROL_IMAGE           XTK_CONTROL_IMAGE
#define FLUX_CONTROL_PROGRESS        XTK_CONTROL_PROGRESS
#define FLUX_CONTROL_LIST            XTK_CONTROL_LIST
#define FLUX_CONTROL_TAB             XTK_CONTROL_TAB
#define FLUX_CONTROL_CARD            XTK_CONTROL_CARD
#define FLUX_CONTROL_DIVIDER         XTK_CONTROL_DIVIDER
#define FLUX_CONTROL_CANVAS          XTK_CONTROL_CANVAS
#define FLUX_CONTROL_PASSWORD_BOX    XTK_CONTROL_PASSWORD_BOX
#define FLUX_CONTROL_NUMBER_BOX      XTK_CONTROL_NUMBER_BOX
#define FLUX_CONTROL_HYPERLINK       XTK_CONTROL_HYPERLINK
#define FLUX_CONTROL_REPEAT_BUTTON   XTK_CONTROL_REPEAT_BUTTON
#define FLUX_CONTROL_PROGRESS_RING   XTK_CONTROL_PROGRESS_RING
#define FLUX_CONTROL_INFO_BADGE      XTK_CONTROL_INFO_BADGE
#define FLUX_CONTROL_TOOLTIP         XTK_CONTROL_TOOLTIP
#define FLUX_CONTROL_FLYOUT          XTK_CONTROL_FLYOUT
#define FLUX_CONTROL_MENU_FLYOUT     XTK_CONTROL_MENU_FLYOUT
#define FLUX_CONTROL_DROPDOWN_BUTTON XTK_CONTROL_DROPDOWN_BUTTON
#define FLUX_CONTROL_SPLIT_BUTTON    XTK_CONTROL_SPLIT_BUTTON
#define FLUX_CONTROL_COMBO_BOX       XTK_CONTROL_COMBO_BOX
#define FLUX_CONTROL_EXPANDER        XTK_CONTROL_EXPANDER
#define FLUX_CONTROL_EXPANDER_HEADER  XTK_CONTROL_EXPANDER_HEADER
#define FLUX_CONTROL_EXPANDER_CONTENT XTK_CONTROL_EXPANDER_CONTENT
#define FLUX_CONTROL_INFO_BAR        XTK_CONTROL_INFO_BAR
#define FLUX_CONTROL_CONTENT_DIALOG  XTK_CONTROL_CONTENT_DIALOG
#define FLUX_CONTROL_DIALOG_CONTENT  XTK_CONTROL_DIALOG_CONTENT
#define FLUX_CONTROL_MENU_BAR        XTK_CONTROL_MENU_BAR
#define FLUX_CONTROL_MENU_BAR_ITEM   XTK_CONTROL_MENU_BAR_ITEM
#define FLUX_CONTROL_NAV_VIEW        XTK_CONTROL_NAV_VIEW
#define FLUX_CONTROL_NAV_VIEW_ITEM   XTK_CONTROL_NAV_VIEW_ITEM
#define FLUX_CONTROL_TAB_VIEW        XTK_CONTROL_TAB_VIEW
#define FLUX_CONTROL_TAB_VIEW_ITEM   XTK_CONTROL_TAB_VIEW_ITEM
#define FLUX_CONTROL_CUSTOM          XTK_CONTROL_CUSTOM

/** @brief Helper: store control type into xent-core's generic node tag. */
static void inline flux_set_control_type(XentContext *ctx, XentNodeId node, FluxControlType type) {
	xent_set_node_tag(ctx, node, ( uint8_t ) type);
}

/** @brief Helper: read control type from xent-core's generic node tag. */
static FluxControlType inline flux_get_control_type(XentContext const *ctx, XentNodeId node) {
	return ( FluxControlType ) xent_get_node_tag(ctx, node);
}

#ifdef __cplusplus
}
#endif

#endif
