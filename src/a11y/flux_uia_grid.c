#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"

#define UIA_GRID_UNSUPPORTED HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)

static FluxUiaSub const *uia_grid_sub(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h ? h->sub : NULL;
}

static IRawElementProviderSimple *uia_grid_simple(FluxUia const *self, int sub) {
	FluxUia *p = flux_uia_alloc_sub(&self->info, self->node, sub);
	return p ? ( IRawElementProviderSimple * ) &p->lpVtblSimple : NULL;
}

static HRESULT uia_grid_run(FluxUia const *self, int base, int count, SAFEARRAY **out) {
	if (!out) return E_POINTER;
	*out = SafeArrayCreateVector(VT_UNKNOWN, 0, ( ULONG ) (count > 0 ? count : 0));
	if (!*out) return E_OUTOFMEMORY;
	for (LONG i = 0; i < count; i++) {
		IRawElementProviderSimple *item = uia_grid_simple(self, base + ( int ) i);
		if (!item) continue;
		SafeArrayPutElement(*out, &i, ( IUnknown * ) item);
		item->lpVtbl->Release(item);
	}
	return S_OK;
}

static bool uia_grid_cell(FluxUia const *self, int *row, int *col) {
	FluxUiaSub const *s = uia_grid_sub(self);
	return self->sub >= 0 && s && s->cell && s->cell(self->info.store, self->node, self->sub, row, col);
}

static HRESULT STDMETHODCALLTYPE grid_qi(IGridProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblGrid), riid, ppv);
}

static ULONG STDMETHODCALLTYPE grid_addref(IGridProvider *p) { return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblGrid)); }

static ULONG STDMETHODCALLTYPE grid_release(IGridProvider *p) { return flux_uia_release(FLUX_UIA_SELF(p, lpVtblGrid)); }

static int                     uia_grid_find(FluxUia const *self, int row, int col) {
	int first = 0, last = -1, r = 0, c = 0;
	if (!flux_uia_sub_range(&self->info, self->node, &first, &last)) return -1;
	FluxUia probe = {.info = self->info, .node = self->node};
	for (probe.sub = first; probe.sub <= last; probe.sub++)
		if (uia_grid_cell(&probe, &r, &c) && r == row && c == col) return probe.sub;
	return -1;
}

static HRESULT STDMETHODCALLTYPE grid_item(IGridProvider *p, int row, int col, IRawElementProviderSimple **out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblGrid);
	if (!out) return E_POINTER;
	int sub = uia_grid_find(self, row, col);
	*out    = sub >= 0 ? uia_grid_simple(self, sub) : NULL;
	return S_OK;
}

static void uia_grid_dims(FluxUia const *self, int *rows, int *cols) {
	FluxUiaSub const *s = uia_grid_sub(self);
	*rows = *cols = 0;
	if (s && s->grid) s->grid(self->info.store, self->node, rows, cols);
}

static HRESULT STDMETHODCALLTYPE grid_rows(IGridProvider *p, int *out) {
	int cols = 0;
	if (!out) return E_POINTER;
	uia_grid_dims(FLUX_UIA_SELF(p, lpVtblGrid), out, &cols);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE grid_cols(IGridProvider *p, int *out) {
	int rows = 0;
	if (!out) return E_POINTER;
	uia_grid_dims(FLUX_UIA_SELF(p, lpVtblGrid), &rows, out);
	return S_OK;
}

IGridProviderVtbl const flux_uia_grid_vtbl = {
  grid_qi,
  grid_addref,
  grid_release,
  grid_item,
  grid_rows,
  grid_cols,
};

static HRESULT STDMETHODCALLTYPE gi_qi(IGridItemProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblGridItem), riid, ppv);
}

static ULONG STDMETHODCALLTYPE gi_addref(IGridItemProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblGridItem));
}

static ULONG STDMETHODCALLTYPE gi_release(IGridItemProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblGridItem));
}

static HRESULT uia_grid_axis(IGridItemProvider *p, int *out, bool column) {
	int row = 0, col = 0;
	if (!out) return E_POINTER;
	*out = 0;
	if (!uia_grid_cell(FLUX_UIA_SELF(p, lpVtblGridItem), &row, &col)) return UIA_GRID_UNSUPPORTED;
	*out = column ? col : row;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE gi_row(IGridItemProvider *p, int *out) { return uia_grid_axis(p, out, false); }

static HRESULT STDMETHODCALLTYPE gi_col(IGridItemProvider *p, int *out) { return uia_grid_axis(p, out, true); }

static HRESULT STDMETHODCALLTYPE gi_span(IGridItemProvider *p, int *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = 1;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE gi_grid(IGridItemProvider *p, IRawElementProviderSimple **out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblGridItem);
	if (!out) return E_POINTER;
	FluxUia *grid = flux_uia_alloc(&self->info, self->node);
	*out          = grid ? ( IRawElementProviderSimple * ) &grid->lpVtblSimple : NULL;
	return S_OK;
}

IGridItemProviderVtbl const flux_uia_griditem_vtbl = {
  gi_qi,
  gi_addref,
  gi_release,
  gi_row,
  gi_col,
  gi_span,
  gi_span,
  gi_grid,
};

static HRESULT STDMETHODCALLTYPE tbl_qi(ITableProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblTable), riid, ppv);
}

static ULONG STDMETHODCALLTYPE tbl_addref(ITableProvider *p) { return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblTable)); }

static ULONG STDMETHODCALLTYPE tbl_release(ITableProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblTable));
}

static HRESULT STDMETHODCALLTYPE tbl_rows(ITableProvider *p, SAFEARRAY **out) {
	return uia_grid_run(FLUX_UIA_SELF(p, lpVtblTable), 0, 0, out);
}

static int uia_grid_base(FluxUia const *self) {
	FluxUiaSub const *s = uia_grid_sub(self);
	return s ? s->column_head : 0;
}

static HRESULT STDMETHODCALLTYPE tbl_cols(ITableProvider *p, SAFEARRAY **out) {
	FluxUia *self  = FLUX_UIA_SELF(p, lpVtblTable);
	int      base  = uia_grid_base(self);
	int      count = flux_uia_sub_heads(self) - base;
	return uia_grid_run(self, FLUX_UIA_SUB_HEAD + base, count > 0 ? count : 0, out);
}

static HRESULT STDMETHODCALLTYPE tbl_major(ITableProvider *p, enum RowOrColumnMajor *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = RowOrColumnMajor_RowMajor;
	return S_OK;
}

ITableProviderVtbl const flux_uia_table_vtbl = {
  tbl_qi,
  tbl_addref,
  tbl_release,
  tbl_rows,
  tbl_cols,
  tbl_major,
};

static HRESULT STDMETHODCALLTYPE ti_qi(ITableItemProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblTableItem), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ti_addref(ITableItemProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblTableItem));
}

static ULONG STDMETHODCALLTYPE ti_release(ITableItemProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblTableItem));
}

static HRESULT STDMETHODCALLTYPE ti_rows(ITableItemProvider *p, SAFEARRAY **out) {
	FluxUia          *self = FLUX_UIA_SELF(p, lpVtblTableItem);
	FluxUiaSub const *s    = uia_grid_sub(self);
	int               head = s && s->row_head ? s->row_head(self->info.store, self->node, self->sub) : -1;
	return uia_grid_run(self, head, head >= 0 ? 1 : 0, out);
}

static HRESULT STDMETHODCALLTYPE ti_cols(ITableItemProvider *p, SAFEARRAY **out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblTableItem);
	int      row = 0, col = 0;
	int      base = uia_grid_base(self);
	bool     has  = uia_grid_cell(self, &row, &col) && base + col < flux_uia_sub_heads(self);
	return uia_grid_run(self, FLUX_UIA_SUB_HEAD + base + col, has ? 1 : 0, out);
}

ITableItemProviderVtbl const flux_uia_tableitem_vtbl = {
  ti_qi,
  ti_addref,
  ti_release,
  ti_rows,
  ti_cols,
};
