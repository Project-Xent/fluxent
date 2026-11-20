#ifndef FLUX_UIA_OBJECT_H
#define FLUX_UIA_OBJECT_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#include "a11y/flux_uia.h"

#include <windows.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

#include <stddef.h>

typedef struct FluxUia {
	IRawElementProviderSimpleVtbl const       *lpVtblSimple;
	IRawElementProviderFragmentVtbl const     *lpVtblFragment;
	IRawElementProviderFragmentRootVtbl const *lpVtblRoot;
	IInvokeProviderVtbl const                 *lpVtblInvoke;
	IToggleProviderVtbl const                 *lpVtblToggle;
	IRangeValueProviderVtbl const             *lpVtblRange;
	IValueProviderVtbl const                  *lpVtblValue;
	ISelectionItemProviderVtbl const          *lpVtblSelection;
	IScrollProviderVtbl const                 *lpVtblScroll;
	IScrollItemProviderVtbl const             *lpVtblScrollItem;
	LONG                                       ref;
	FluxUiaContext                             info;
	XentNodeId                                 node;
	int                                        thumb;
} FluxUia;

#define FLUX_UIA_SELF(p, field) (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, field)))

HRESULT                              flux_uia_qi(FluxUia *self, REFIID riid, void **ppv);
ULONG                                flux_uia_addref(FluxUia *self);
ULONG                                flux_uia_release(FluxUia *self);

extern IScrollProviderVtbl const     flux_uia_scroll_vtbl;
extern IScrollItemProviderVtbl const flux_uia_scrollitem_vtbl;

#endif
