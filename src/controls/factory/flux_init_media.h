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
	char const      *source;
	FluxImageStretch stretch;
} FluxImageDesc;

XentNodeId flux_image_init(FluxImageDesc const *info);

typedef struct FluxInfoBadgeDesc {
	XentCtx          *ctx;
	FluxStore        *store;
	XentNodeId        parent;
	FluxInfoBadgeMode mode;
	int32_t           value;
} FluxInfoBadgeDesc;

XentNodeId flux_badge_init(FluxInfoBadgeDesc const *info);
void flux_badge_configure(FluxStore *store, XentNodeId id, FluxInfoBadgeMode mode, int32_t value, char const *icon);

typedef struct FluxPersonPictureDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	float       diameter;
	char const *display_name;
	char const *initials;
	char const *image_path;
	char const *badge_glyph;
	int         badge_number;
	bool        is_group;
} FluxPersonPictureDesc;

XentNodeId flux_avatar_init(FluxPersonPictureDesc const *info);

void       flux_avatar_setname(FluxStore *store, XentNodeId pp, char const *display_name);

void       flux_avatar_set_initials(FluxStore *store, XentNodeId pp, char const *initials);

void       flux_avatar_set_image(FluxStore *store, XentNodeId pp, char const *image_path);

void       flux_avatar_set_badge(FluxStore *store, XentNodeId pp, char const *badge_glyph, int badge_number);

void       flux_avatar_set_group(FluxStore *store, XentNodeId pp, bool is_group);

#endif
