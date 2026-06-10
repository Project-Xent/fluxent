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

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum FluxControlType
{
	FLUX_CONTROL_CONTAINER = 0,
	FLUX_CONTROL_TEXT,
	FLUX_CONTROL_BUTTON,
	FLUX_CONTROL_TOGGLE_BUTTON,
	FLUX_CONTROL_CHECKBOX,
	FLUX_CONTROL_RADIO,
	FLUX_CONTROL_SWITCH,
	FLUX_CONTROL_SLIDER,
	FLUX_CONTROL_TEXT_INPUT,
	FLUX_CONTROL_SCROLL,
	FLUX_CONTROL_IMAGE,
	FLUX_CONTROL_PROGRESS,
	FLUX_CONTROL_LIST,
	FLUX_CONTROL_TAB,
	FLUX_CONTROL_CARD,
	FLUX_CONTROL_DIVIDER,
	FLUX_CONTROL_CANVAS,
	FLUX_CONTROL_PASSWORD_BOX,
	FLUX_CONTROL_NUMBER_BOX,
	FLUX_CONTROL_HYPERLINK,
	FLUX_CONTROL_REPEAT_BUTTON,
	FLUX_CONTROL_PROGRESS_RING,
	FLUX_CONTROL_INFO_BADGE,
	FLUX_CONTROL_TOOLTIP,
	FLUX_CONTROL_FLYOUT,
	FLUX_CONTROL_MENU_FLYOUT,
	FLUX_CONTROL_DROPDOWN_BUTTON,
	FLUX_CONTROL_SPLIT_BUTTON,
	FLUX_CONTROL_COMBO_BOX,
	FLUX_CONTROL_EXPANDER,
	FLUX_CONTROL_EXPANDER_HEADER,
	FLUX_CONTROL_EXPANDER_CONTENT,
	FLUX_CONTROL_INFO_BAR,
	FLUX_CONTROL_CONTENT_DIALOG,
	FLUX_CONTROL_DIALOG_CONTENT,
	FLUX_CONTROL_CUSTOM,
} FluxControlType;

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
