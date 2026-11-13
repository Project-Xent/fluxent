#ifndef FLUX_INFO_BADGE_DATA_H
#define FLUX_INFO_BADGE_DATA_H

#include <fluxent/flux_types.h>

#include <stdint.h>

typedef XtkBadgeMode FluxInfoBadgeMode;

#define FLUX_BADGE_DOT    XTK_BADGE_DOT
#define FLUX_BADGE_NUMBER XTK_BADGE_NUMBER
#define FLUX_BADGE_ICON   XTK_BADGE_ICON

typedef struct FluxInfoBadgeData {
	FluxInfoBadgeMode mode;
	int32_t           value;
	char const       *icon_name;
	FluxColor         background;
} FluxInfoBadgeData;

#endif
