#ifndef FLUX_A11Y_UIA_INTERNAL_H
#define FLUX_A11Y_UIA_INTERNAL_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif
#include <windows.h>

typedef struct FluxWindow FluxWindow;

/** @brief Handle WM_GETOBJECT via the UIA provider callback; 0 if not applicable. */
LRESULT                   flux_uia_get_object(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp);

#endif
