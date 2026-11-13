#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#include "a11y/flux_uia_internal.h"
#include "window/flux_window_internal.h"

#include <windows.h>
#include <objbase.h>
#define UIA_ScrollPatternNoScroll flux_uia_ignored_scroll_no_scroll
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

LRESULT flux_uia_get_object(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp) {
	if (!win || !win->on_uia_provider || lp != ( LPARAM ) UiaRootObjectId) return 0;

	void *prov = win->on_uia_provider(win->uia_provider_ctx, hwnd);
	if (!prov) return 0;

	LRESULT res = UiaReturnRawElementProvider(hwnd, wp, lp, ( IRawElementProviderSimple * ) prov);
	(( IUnknown * ) prov)->lpVtbl->Release(( IUnknown * ) prov);
	return res;
}
