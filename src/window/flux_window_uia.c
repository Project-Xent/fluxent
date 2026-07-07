/* WM_GETOBJECT bridge to the UIA provider callback. Isolated here so the
 * CINTERFACE Windows.UI.Automation headers do not affect the rest of the
 * window implementation. */
#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#include "flux_window_internal.h"

#include <windows.h>
/* WIN32_LEAN_AND_MEAN keeps windows.h from pulling the COM headers, so `interface`
 * (used by the UIA C headers) is undefined under MSVC; pull it in explicitly. */
#include <objbase.h>
/* uiautomationcore.h defines UIA_ScrollPatternNoScroll as a file-scope const double,
 * which has external linkage in C and collides with flux_uia.c's copy at link
 * (LNK2005 under MSVC). This bridge never uses it; give this TU's copy a private name. */
#define UIA_ScrollPatternNoScroll flux_uia_ignored_scroll_no_scroll
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

LRESULT flux_window_uia_get_object(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp) {
	if (!win || !win->on_uia_provider || lp != ( LPARAM ) UiaRootObjectId) return 0;

	void *prov = win->on_uia_provider(win->uia_provider_ctx, hwnd);
	if (!prov) return 0;

	LRESULT res = UiaReturnRawElementProvider(hwnd, wp, lp, ( IRawElementProviderSimple * ) prov);
	(( IUnknown * ) prov)->lpVtbl->Release(( IUnknown * ) prov);
	return res;
}
