#ifndef FLUX_TOAST_H
#define FLUX_TOAST_H

#include "flux_window.h"

/** Shows Windows toast XML. Desktop apps supply the AppUserModelID registered by their installer. */
HRESULT flux_toast_show(wchar_t const *app_id, wchar_t const *xml);

#endif
