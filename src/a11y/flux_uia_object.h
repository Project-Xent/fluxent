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
	IExpandCollapseProviderVtbl const         *lpVtblExpand;
	ISelectionProviderVtbl const              *lpVtblSelectionC;
	LONG                                       ref;
	FluxUiaContext                             info;
	XentNodeId                                 node;
	int                                        thumb;
} FluxUia;

#define FLUX_UIA_SELF(p, field) (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, field)))

FluxUia                                 *flux_uia_alloc(FluxUiaContext const *info, XentNodeId node);
HRESULT                                  flux_uia_activate(FluxUia *self);
unsigned                                 flux_uia_patterns(FluxUiaContext const *info, XentNodeId node);
HRESULT                                  flux_uia_qi(FluxUia *self, REFIID riid, void **ppv);
ULONG                                    flux_uia_addref(FluxUia *self);
ULONG                                    flux_uia_release(FluxUia *self);

extern IScrollProviderVtbl const         flux_uia_scroll_vtbl;
extern IScrollItemProviderVtbl const     flux_uia_scrollitem_vtbl;
extern IExpandCollapseProviderVtbl const flux_uia_expand_vtbl;
extern ISelectionProviderVtbl const      flux_uia_selection_vtbl;
extern ISelectionItemProviderVtbl const  flux_uia_selitem_vtbl;

#endif
