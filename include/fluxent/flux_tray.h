#ifndef FLUX_TRAY_H
#define FLUX_TRAY_H

#include "flux_window.h"

typedef struct FluxTrayIcon FluxTrayIcon;
typedef void                (*FluxTrayActivateFn)(void *ctx);
typedef void                (*FluxTrayCommandFn)(void *ctx, uint32_t command);

typedef struct FluxTrayDesc {
	HICON              icon;
	wchar_t const     *tooltip;
	HMENU              menu;
	FluxTrayActivateFn activate;
	FluxTrayCommandFn  command;
	void              *ctx;
} FluxTrayDesc;

/** Copies icon and tooltip. Borrows menu and ctx until update, removal or window destruction. */
HRESULT flux_tray_create(FluxWindow *win, FluxTrayDesc const *desc, FluxTrayIcon **out);
HRESULT flux_tray_update(FluxTrayIcon *tray, FluxTrayDesc const *desc);
void    flux_tray_destroy(FluxTrayIcon *tray);

#endif
