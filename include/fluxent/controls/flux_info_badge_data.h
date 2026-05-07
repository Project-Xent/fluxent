/**
 * @file flux_info_badge_data.h
 * @brief Data structure for FluxInfoBadge control.
 */
#ifndef FLUX_INFO_BADGE_DATA_H
#define FLUX_INFO_BADGE_DATA_H

#include "../flux_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Display modes for an info badge. */
typedef enum FluxInfoBadgeMode
{
	FLUX_BADGE_DOT,    /**< Simple colored dot (no content) */
	FLUX_BADGE_NUMBER, /**< Displays a numeric value */
	FLUX_BADGE_ICON,   /**< Displays an icon */
} FluxInfoBadgeMode;

/**
 * @brief Configuration for an info badge (notification indicator).
 *
 * Typically overlaid on icons or buttons to show counts or status.
 */
typedef struct FluxInfoBadgeData {
	FluxInfoBadgeMode mode;       /**< Badge display mode */
	int32_t           value;      /**< Number to display (NUMBER mode) */
	char const       *icon_name;  /**< Icon name (ICON mode) */
	FluxColor         background; /**< Badge background color */
} FluxInfoBadgeData;

#ifdef __cplusplus
}
#endif

#endif
