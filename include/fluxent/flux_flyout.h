#ifndef FLUX_FLYOUT_H
#define FLUX_FLYOUT_H

#include "flux_types.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_window.h"

#define FLUX_FLYOUT_PAD_LEFT      16.0f
#define FLUX_FLYOUT_PAD_TOP       15.0f
#define FLUX_FLYOUT_PAD_RIGHT     16.0f
#define FLUX_FLYOUT_PAD_BOTTOM    17.0f
#define FLUX_FLYOUT_CORNER_RADIUS 8.0f
#define FLUX_FLYOUT_BORDER_WIDTH  1.0f
#define FLUX_FLYOUT_OFFSET        4.0f

typedef struct FluxFlyout  FluxFlyout;

typedef void               (*FluxFlyoutPaintFn)(void *user_ctx, FluxFlyout *flyout, FluxRect const *content_rect);

XENT_NODISCARD FluxFlyout *flux_flyout_create(FluxWindow *owner);

void                       flux_flyout_destroy(FluxFlyout *flyout);

void                       flux_flyout_settext(FluxFlyout *flyout, FluxText *tr);

void                       flux_flyout_settheme(FluxFlyout *flyout, FluxThemeColors const *theme, bool is_dark);

void                       flux_flyout_setthememanager(FluxFlyout *flyout, FluxTheme *tm);

void                       flux_flyout_setsize(FluxFlyout *flyout, float width, float height);

void                       flux_flyout_onpaint(FluxFlyout *flyout, FluxFlyoutPaintFn cb, void *user_ctx);

void                       flux_flyout_ondismiss(FluxFlyout *flyout, FluxPopupDismissFn cb, void *user_ctx);

void                       flux_flyout_show(FluxFlyout *flyout, FluxRect anchor, FluxPlacement placement);

void                       flux_flyout_dismiss(FluxFlyout *flyout);

bool                       flux_flyout_visible(FluxFlyout const *flyout);

FluxGfx                   *flux_flyout_gfx(FluxFlyout *flyout);

FluxWindow                *flux_flyout_owner(FluxFlyout *flyout);

FluxPopup                 *flux_flyout_popup(FluxFlyout *flyout);

FluxThemeColors const     *flux_flyout_theme(FluxFlyout const *flyout);

bool                       flux_flyout_dark(FluxFlyout const *flyout);

FluxText                  *flux_flyout_text(FluxFlyout *flyout);

#endif
