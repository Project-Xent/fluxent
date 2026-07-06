/**
 * @file flux_person_picture_data.h
 * @brief Data for FluxPersonPicture — a circular avatar (WinUI 3 PersonPicture).
 *
 * A circle that shows, in priority order, a profile photo, the person's
 * initials, or a placeholder glyph (Contact when empty, People in group mode).
 * Initials are resolved from an explicit override or derived from the display
 * name (first + last word). An optional top-right badge shows a number (capped
 * "99+"), a glyph, or nothing. Geometry follows the PersonPicture design
 * guidance: initials at 42% of the diameter, a badge plate at 50% with its own
 * text at 60% of the plate.
 */
#ifndef FLUX_PERSON_PICTURE_DATA_H
#define FLUX_PERSON_PICTURE_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore FluxNodeStore;

/** @brief WinUI PersonPicture metric ratios. */
#define FLUX_PP_DEFAULT_SIZE   72.0f /**< Default diameter (DIP). */
#define FLUX_PP_INITIALS_RATIO 0.42f /**< Initials font = 42% of diameter. */
#define FLUX_PP_BADGE_RATIO    0.5f  /**< Badge plate = 50% of diameter. */
#define FLUX_PP_BADGE_FONT     0.6f  /**< Badge text/glyph = 60% of the plate. */
#define FLUX_PP_GLYPH_CONTACT  0xE77Bu /**< Placeholder "Contact" glyph. */
#define FLUX_PP_GLYPH_GROUP    0xE716u /**< Placeholder "People" glyph. */

/** @brief Retained state for a FLUX_CONTROL_PERSON_PICTURE node. */
typedef struct FluxPersonPictureData {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     node;

	float          diameter;      /**< Circle diameter (DIP). */
	char          *display_name;  /**< Owned; source for derived initials. */
	char          *initials;      /**< Owned explicit override, or NULL. */
	char          *image_path;    /**< Owned profile-photo source path, or NULL. */
	char          *badge_glyph;   /**< Owned UTF-8 badge glyph (resolved), or NULL. */
	int            badge_number;  /**< Badge count; <= 0 hides the number. */
	bool           is_group;      /**< Group mode: People placeholder glyph. */

	char           resolved [12]; /**< Initials actually drawn (explicit or derived). */
} FluxPersonPictureData;

#ifdef __cplusplus
}
#endif

#endif
