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
