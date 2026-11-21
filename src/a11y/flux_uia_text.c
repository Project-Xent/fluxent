#define UIA_ScrollPatternNoScroll flux_uia_noscroll_text
#include "a11y/flux_uia_doc.h"
#include "store/flux_node_data.h"

#include <stdlib.h>

static FluxUia                  *text_self(ITextProvider2 *p) { return FLUX_UIA_SELF(p, lpVtblText); }

static HRESULT STDMETHODCALLTYPE text_qi(ITextProvider2 *p, REFIID riid, void **ppv) {
	return flux_uia_qi(text_self(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE text_addref(ITextProvider2 *p) { return flux_uia_addref(text_self(p)); }

static ULONG STDMETHODCALLTYPE text_release(ITextProvider2 *p) { return flux_uia_release(text_self(p)); }

static bool                    text_open(FluxUia *self, UiaDoc *doc) {
	if (flux_uia_doc_open(&self->info, self->node, doc)) return true;
	flux_uia_doc_close(doc);
	return false;
}

static ITextRangeProvider *text_range(FluxUia const *self, uint32_t start, uint32_t end) {
	return flux_uia_range_new(&self->info, self->node, start, end);
}

static HRESULT text_one(FluxUia const *self, uint32_t start, uint32_t end, ITextRangeProvider **out) {
	if (!out) return E_POINTER;
	*out = text_range(self, start, end);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE text_selection(ITextProvider2 *p, SAFEARRAY **out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!text_open(self, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	ITextRangeProvider *one [1] = {NULL};
	uint32_t            a = flux_uia_doc_to16(&doc, doc.view.sel_start), b = flux_uia_doc_to16(&doc, doc.view.sel_end);
	if (doc.view.selectable) one [0] = text_range(self, a, b);
	flux_uia_doc_close(&doc);
	*out = flux_uia_range_array(one, doc.view.selectable ? 1u : 0u);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE text_visible(ITextProvider2 *p, SAFEARRAY **out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!text_open(self, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	ITextRangeProvider **all = ( ITextRangeProvider ** ) calloc(doc.nline, sizeof(*all));
	uint32_t             n   = 0u;
	for (uint32_t i = 0; all && i < doc.nline; i++)
		if (flux_uia_doc_line_shown(&doc, i))
			all [n++] = text_range(self, doc.lines [i].start, doc.lines [i].start + doc.lines [i].length);
	flux_uia_doc_close(&doc);
	*out = flux_uia_range_array(all, n);
	free(all);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE text_from_child(
  ITextProvider2 *p, IRawElementProviderSimple *child, ITextRangeProvider **out
) {
	( void ) p;
	( void ) child;
	if (!out) return E_POINTER;
	*out = NULL;
	return E_INVALIDARG;
}

static HRESULT STDMETHODCALLTYPE text_from_point(ITextProvider2 *p, struct UiaPoint pt, ITextRangeProvider **out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!text_open(self, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	uint32_t at = flux_uia_doc_at(&doc, pt.x, pt.y);
	flux_uia_doc_close(&doc);
	return text_one(self, at, at, out);
}

static HRESULT STDMETHODCALLTYPE text_document(ITextProvider2 *p, ITextRangeProvider **out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!text_open(self, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	uint32_t len = doc.len;
	flux_uia_doc_close(&doc);
	return text_one(self, 0u, len, out);
}

static HRESULT STDMETHODCALLTYPE text_supported(ITextProvider2 *p, enum SupportedTextSelection *out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!out) return E_POINTER;
	*out = SupportedTextSelection_None;
	if (!text_open(self, &doc)) return S_OK;
	if (doc.view.selectable) *out = SupportedTextSelection_Single;
	flux_uia_doc_close(&doc);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE text_from_annotation(
  ITextProvider2 *p, IRawElementProviderSimple *annotation, ITextRangeProvider **out
) {
	return text_from_child(p, annotation, out);
}

static bool text_focused(FluxUia const *self) {
	FluxNodeData const *nd = flux_store_find(self->info.store, self->node);
	return nd && nd->state.focused && GetFocus() == self->info.hwnd;
}

static HRESULT STDMETHODCALLTYPE text_caret(ITextProvider2 *p, BOOL *active, ITextRangeProvider **out) {
	FluxUia *self = text_self(p);
	UiaDoc   doc;
	if (!active || !out) return E_POINTER;
	*out    = NULL;
	*active = text_focused(self);
	if (!text_open(self, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	uint32_t at = flux_uia_doc_to16(&doc, doc.view.sel_end);
	flux_uia_doc_close(&doc);
	return text_one(self, at, at, out);
}

ITextProvider2Vtbl const flux_uia_text_vtbl = {
  text_qi,
  text_addref,
  text_release,
  text_selection,
  text_visible,
  text_from_child,
  text_from_point,
  text_document,
  text_supported,
  text_from_annotation,
  text_caret,
};
