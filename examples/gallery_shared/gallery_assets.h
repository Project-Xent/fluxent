#ifndef GALLERY_ASSETS_H
#define GALLERY_ASSETS_H

#include <fluxent/flux_host.h>

void        gallery_assets_init(void);
void        gallery_assets_clear(void);
XtkResource gallery_asset(wchar_t const *name);
XtkResource gallery_wallpaper(void);

#endif
