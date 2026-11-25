#ifndef FLUX_UIA_OBJECT_H
#define FLUX_UIA_OBJECT_H

#include "a11y/flux_uia_sdk.h"
#include "a11y/flux_uia.h"

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
	IWindowProviderVtbl const                 *lpVtblWindow;
	IGridProviderVtbl const                   *lpVtblGrid;
	IGridItemProviderVtbl const               *lpVtblGridItem;
	ITableProviderVtbl const                  *lpVtblTable;
	ITableItemProviderVtbl const              *lpVtblTableItem;
	IItemContainerProviderVtbl const          *lpVtblItems;
	IVirtualizedItemProviderVtbl const        *lpVtblVirtual;
	ITextProvider2Vtbl const                  *lpVtblText;
	IDropTargetProviderVtbl const             *lpVtblDrop;
	LONG                                       ref;
	FluxUiaContext                             info;
	XentNodeId                                 node;
	int                                        sub;
} FluxUia;

#define FLUX_UIA_SELF(p, field) (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, field)))

FluxUia                         *flux_uia_alloc(FluxUiaContext const *info, XentNodeId node);
HRESULT                          flux_uia_activate(FluxUia *self);
unsigned                         flux_uia_patterns(FluxUiaContext const *info, XentNodeId node);
FluxUia                         *flux_uia_alloc_sub(FluxUiaContext const *info, XentNodeId node, int sub);
HRESULT                          flux_uia_qi(FluxUia *self, REFIID riid, void **ppv);
bool                             flux_uia_sub_range(FluxUiaContext const *info, XentNodeId node, int *first, int *last);
ULONG                            flux_uia_addref(FluxUia *self);
ULONG                            flux_uia_release(FluxUia *self);

extern IScrollProviderVtbl const flux_uia_scroll_vtbl;
extern IScrollItemProviderVtbl const      flux_uia_scrollitem_vtbl;
extern IExpandCollapseProviderVtbl const  flux_uia_expand_vtbl;
extern ISelectionProviderVtbl const       flux_uia_selection_vtbl;
extern ISelectionItemProviderVtbl const   flux_uia_selitem_vtbl;
extern IWindowProviderVtbl const          flux_uia_window_vtbl;
extern IGridProviderVtbl const            flux_uia_grid_vtbl;
extern IGridItemProviderVtbl const        flux_uia_griditem_vtbl;
extern ITableProviderVtbl const           flux_uia_table_vtbl;
extern ITableItemProviderVtbl const       flux_uia_tableitem_vtbl;

extern IItemContainerProviderVtbl const   flux_uia_items_vtbl;
extern IVirtualizedItemProviderVtbl const flux_uia_virtual_vtbl;
extern ITextProvider2Vtbl const           flux_uia_text_vtbl;
extern IDropTargetProviderVtbl const      flux_uia_drop_vtbl;

int                                       flux_uia_sub_heads(FluxUia const *self);
FluxUia                                  *flux_uia_unwrap(IRawElementProviderSimple *p);
void       flux_uia_origin(FluxUiaContext const *info, XentNodeId node, POINT *origin, float *scale);
void       flux_uia_bring_into_view(FluxUiaContext const *info, XentNodeId node);
bool       flux_uia_place(FluxUiaContext const *info, XentNodeId node, int sub, XentRect *visual, XentRect *seen);
bool       flux_uia_bounds(FluxUiaContext const *info, XentNodeId node, int sub, struct UiaRect *out);
XentNodeId flux_uia_hit(FluxUiaContext const *info, double x, double y, int *sub);
void       flux_uia_property(FluxUia *self, int role, PROPERTYID id, VARIANT *out);
BSTR       flux_uia_bstr(char const *utf8);
bool       flux_uia_focusable(FluxUia const *self);
XentNodeId flux_uia_find_focused(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_delegate(FluxUiaContext const *info, XentNodeId node);
void       flux_uia_dirty(FluxUiaContext const *info);
int        flux_uia_refs(FluxUiaContext const *info, XentNodeId node, int slot, XentNodeId *out, int cap);
void       flux_uia_refs_prop(FluxUiaContext const *info, XentNodeId node, int slot, XentNodeId extra, VARIANT *out);
wchar_t const *flux_uia_class(FluxUiaContext const *info, XentNodeId node, int sub, int role);
wchar_t const *flux_uia_lctype(FluxUiaContext const *info, XentNodeId node, int sub);

#endif
