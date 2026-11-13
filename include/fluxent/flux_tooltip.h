#ifndef FLUX_TOOLTIP_H
#define FLUX_TOOLTIP_H

#include "flux_types.h"
#include "flux_window.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_node_store.h"
#include <xent/xent.h>

#define FLUX_TOOLTIP_DELAY_MS      800
#define FLUX_TOOLTIP_MAX_WIDTH     320.0f
#define FLUX_TOOLTIP_FONT_SIZE     12.0f
#define FLUX_TOOLTIP_PAD_LEFT      9.0f
#define FLUX_TOOLTIP_PAD_TOP       6.0f
#define FLUX_TOOLTIP_PAD_RIGHT     9.0f
#define FLUX_TOOLTIP_PAD_BOTTOM    8.0f
#define FLUX_TOOLTIP_CORNER_RADIUS 4.0f
#define FLUX_TOOLTIP_BORDER_WIDTH  1.0f
#define FLUX_TOOLTIP_OFFSET        4.0f

typedef struct FluxTooltip  FluxTooltip;

XENT_NODISCARD FluxTooltip *flux_tooltip_create(FluxWindow *owner);

void                        flux_tooltip_destroy(FluxTooltip *tt);

void                        flux_tooltip_settext(FluxTooltip *tt, FluxText *tr);

void                        flux_tooltip_settheme(FluxTooltip *tt, FluxThemeColors const *theme, bool is_dark);

void flux_tooltip_hover(FluxTooltip *tt, XentNodeId hovered, FluxNodeData const *nd, FluxRect const *screen_bounds);

void flux_tooltip_dismiss(FluxTooltip *tt);

void flux_tooltip_showat(FluxTooltip *tt, char const *text, FluxRect anchor, FluxPlacement placement);

bool flux_tooltip_visible(FluxTooltip const *tt);

#endif
