/**
 * @file flux_init_media.h
 * @brief Private Desc + init prototypes for display leaf controls:
 * Image / InfoBadge / PersonPicture.
 */
#ifndef FLUX_INIT_MEDIA_H
#define FLUX_INIT_MEDIA_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxImageDesc {
	XentCtx         *ctx;
	FluxStore       *store;
	XentNodeId       parent;
	char const      *source;  /**< File path / URI. */
	FluxImageStretch stretch; /**< Scaling mode (FLUX_IMAGE_UNIFORM recommended). */
} FluxImageDesc;

/** @brief Create an image node (WIC-decoded bitmap with a stretch mode). */
XentNodeId flux_image_init(FluxImageDesc const *info);

typedef struct FluxInfoBadgeDesc {
	XentCtx          *ctx;
	FluxStore        *store;
	XentNodeId        parent;
	FluxInfoBadgeMode mode;
	int32_t           value;
} FluxInfoBadgeDesc;

/** @brief Create an info badge node. */
XentNodeId flux_badge_init(FluxInfoBadgeDesc const *info);
void flux_badge_configure(FluxStore *store, XentNodeId id, FluxInfoBadgeMode mode, int32_t value, char const *icon);

/* -------------------------------------------------------------------------
 * PersonPicture (circular avatar: photo / initials / placeholder + badge)
 * ---------------------------------------------------------------------- */

typedef struct FluxPersonPictureDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	float       diameter;     /**< 0 = default 72. */
	char const *display_name; /**< Derives initials when @c initials is NULL. */
	char const *initials;     /**< Explicit initials override. */
	char const *image_path;   /**< Profile-photo source, or NULL. */
	char const *badge_glyph;  /**< Segoe Fluent Icons name for the badge, or NULL. */
	int         badge_number; /**< Badge count (capped "99+"); <= 0 hides it. */
	bool        is_group;     /**< Group mode (People placeholder). */
} FluxPersonPictureDesc;

/** @brief Create a PersonPicture avatar leaf. */
XentNodeId flux_avatar_init(FluxPersonPictureDesc const *info);

/** @brief Replace the display name (re-derives initials when no override is set). */
void       flux_avatar_setname(FluxStore *store, XentNodeId pp, char const *display_name);

/** @brief Set an explicit initials override (NULL/"" reverts to the derived value). */
void       flux_avatar_set_initials(FluxStore *store, XentNodeId pp, char const *initials);

/** @brief Set the profile-photo source path (NULL clears the photo). */
void       flux_avatar_set_image(FluxStore *store, XentNodeId pp, char const *image_path);

/** @brief Set the badge: an icon name and/or a count (count <= 0 and NULL glyph clears it). */
void       flux_avatar_set_badge(FluxStore *store, XentNodeId pp, char const *badge_glyph, int badge_number);

/** @brief Toggle group mode (People placeholder glyph). */
void       flux_avatar_set_group(FluxStore *store, XentNodeId pp, bool is_group);

#endif
