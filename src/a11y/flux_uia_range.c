#include "a11y/flux_uia_doc.h"

#include <stdlib.h>

#define RANGE_UNSUPPORTED HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)

typedef struct FluxUiaRange {
	ITextRangeProviderVtbl const *lpVtbl;
	LONG                          ref;
	FluxUiaContext                info;
	XentNodeId                    node;
	int                           sub;
	uint32_t                      start;
	uint32_t                      end;
} FluxUiaRange;

static GUID const Range_IID_IUnknown = {
  0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
static GUID const Range_IID_Range = {
  0x5347ad7b, 0xc355, 0x46f8, {0xaf, 0xf5, 0x90, 0x90, 0x33, 0x58, 0x2f, 0x63}
};

static ITextRangeProviderVtbl const g_range_vtbl;

static FluxUiaRange                *range_self(ITextRangeProvider *p) { return ( FluxUiaRange * ) p; }

static FluxUiaRange                *range_unwrap(ITextRangeProvider *p) {
	return p && p->lpVtbl == &g_range_vtbl ? ( FluxUiaRange * ) p : NULL;
}

static bool range_open(FluxUiaRange *r, UiaDoc *doc) {
	if (!flux_uia_doc_open(&r->info, r->node, r->sub, doc)) {
		flux_uia_doc_close(doc);
		return false;
	}
	if (r->end > doc->len) r->end = doc->len;
	if (r->start > r->end) r->start = r->end;
	return true;
}

ITextRangeProvider *
flux_uia_range_new(FluxUiaContext const *info, XentNodeId node, int sub, uint32_t start, uint32_t end) {
	FluxUiaRange *r = ( FluxUiaRange * ) calloc(1, sizeof(*r));
	if (!r) return NULL;
	*r = (FluxUiaRange) {&g_range_vtbl, 1, *info, node, sub, start < end ? start : end, start < end ? end : start};
	return ( ITextRangeProvider * ) r;
}

SAFEARRAY *flux_uia_range_array(ITextRangeProvider **ranges, uint32_t count) {
	SAFEARRAY *sa = SafeArrayCreateVector(VT_UNKNOWN, 0, count);
	for (LONG i = 0; i < ( LONG ) count; i++) {
		if (sa && ranges [i]) SafeArrayPutElement(sa, &i, ( IUnknown * ) ranges [i]);
		if (ranges [i]) ranges [i]->lpVtbl->Release(ranges [i]);
	}
	return sa;
}

static HRESULT STDMETHODCALLTYPE range_qi(ITextRangeProvider *p, REFIID riid, void **ppv) {
	if (!ppv) return E_POINTER;
	*ppv = NULL;
	if (!IsEqualGUID(riid, &Range_IID_IUnknown) && !IsEqualGUID(riid, &Range_IID_Range)) return E_NOINTERFACE;
	*ppv = p;
	InterlockedIncrement(&range_self(p)->ref);
	return S_OK;
}

static ULONG STDMETHODCALLTYPE range_addref(ITextRangeProvider *p) {
	return ( ULONG ) InterlockedIncrement(&range_self(p)->ref);
}

static ULONG STDMETHODCALLTYPE range_release(ITextRangeProvider *p) {
	LONG n = InterlockedDecrement(&range_self(p)->ref);
	if (n == 0) free(p);
	return ( ULONG ) n;
}

static HRESULT range_out(ITextRangeProvider *made, ITextRangeProvider **out) {
	*out = made;
	return made ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE range_clone(ITextRangeProvider *p, ITextRangeProvider **out) {
	FluxUiaRange *r = range_self(p);
	if (!out) return E_POINTER;
	return range_out(flux_uia_range_new(&r->info, r->node, r->sub, r->start, r->end), out);
}

static HRESULT STDMETHODCALLTYPE range_compare(ITextRangeProvider *p, ITextRangeProvider *other, BOOL *out) {
	FluxUiaRange *r = range_self(p), *o = range_unwrap(other);
	if (!out) return E_POINTER;
	if (!o) return E_INVALIDARG;
	*out = o->node == r->node && o->sub == r->sub && o->start == r->start && o->end == r->end;
	return S_OK;
}

static uint32_t range_point(FluxUiaRange const *r, enum TextPatternRangeEndpoint ep) {
	return ep == TextPatternRangeEndpoint_Start ? r->start : r->end;
}

static HRESULT STDMETHODCALLTYPE range_compare_ends(
  ITextRangeProvider *p, enum TextPatternRangeEndpoint ep, ITextRangeProvider *target,
  enum TextPatternRangeEndpoint tep, int *out
) {
	FluxUiaRange *r = range_self(p), *t = range_unwrap(target);
	if (!out) return E_POINTER;
	if (!t || t->node != r->node || t->sub != r->sub) return E_INVALIDARG;
	*out = ( int ) range_point(r, ep) - ( int ) range_point(t, tep);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE range_expand(ITextRangeProvider *p, enum TextUnit unit) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	flux_uia_doc_expand(&doc, unit, r->start, &r->start, &r->end);
	flux_uia_doc_close(&doc);
	return S_OK;
}

static bool range_same(VARIANT *have, VARIANT const *want) {
	VARIANT v;
	VariantInit(&v);
	bool same = have->vt != VT_UNKNOWN
	         && SUCCEEDED(VariantChangeType(&v, ( VARIANT * ) want, 0, have->vt))
	         && VarCmp(have, &v, LOCALE_USER_DEFAULT, 0) == VARCMP_EQ;
	VariantClear(&v);
	return same;
}

static HRESULT STDMETHODCALLTYPE range_find_attr(
  ITextRangeProvider *p, TEXTATTRIBUTEID id, VARIANT val, BOOL backward, ITextRangeProvider **out
) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	VARIANT       have;
	( void ) backward;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	flux_uia_doc_attr(&doc, id, &have);
	bool hit = r->start < r->end && range_same(&have, &val);
	if (have.vt != VT_UNKNOWN) VariantClear(&have);
	flux_uia_doc_close(&doc);
	return hit ? range_out(flux_uia_range_new(&r->info, r->node, r->sub, r->start, r->end), out) : S_OK;
}

static bool range_match(wchar_t const *at, BSTR text, UINT n, bool fold) {
	return CompareStringOrdinal(at, ( int ) n, text, ( int ) n, fold) == CSTR_EQUAL;
}

static int range_search(UiaDoc const *doc, FluxUiaRange const *r, BSTR text, bool backward, bool fold) {
	UINT n = SysStringLen(text);
	if (n == 0 || n > r->end - r->start) return -1;
	int lo = ( int ) r->start, hi = ( int ) (r->end - n);
	for (int k = 0; k <= hi - lo; k++) {
		int at = backward ? hi - k : lo + k;
		if (range_match(doc->wide.text + at, text, n, fold)) return at;
	}
	return -1;
}

static HRESULT STDMETHODCALLTYPE range_find_text(
  ITextRangeProvider *p, BSTR text, BOOL backward, BOOL fold, ITextRangeProvider **out
) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!text) return E_INVALIDARG;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	int at = range_search(&doc, r, text, backward != FALSE, fold != FALSE);
	flux_uia_doc_close(&doc);
	if (at < 0) return S_OK;
	return range_out(
	  flux_uia_range_new(&r->info, r->node, r->sub, ( uint32_t ) at, ( uint32_t ) at + SysStringLen(text)), out
	);
}

static HRESULT STDMETHODCALLTYPE range_attr(ITextRangeProvider *p, TEXTATTRIBUTEID id, VARIANT *out) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!out) return E_POINTER;
	VariantInit(out);
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	flux_uia_doc_attr(&doc, id, out);
	flux_uia_doc_close(&doc);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE range_rects(ITextRangeProvider *p, SAFEARRAY **out) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	*out = flux_uia_doc_rects(&doc, r->start, r->end);
	flux_uia_doc_close(&doc);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE range_enclosing(ITextRangeProvider *p, IRawElementProviderSimple **out) {
	FluxUiaRange *r = range_self(p);
	if (!out) return E_POINTER;
	FluxUia *e = r->sub < 0 ? flux_uia_alloc(&r->info, r->node) : flux_uia_alloc_sub(&r->info, r->node, r->sub);
	*out       = e ? ( IRawElementProviderSimple * ) &e->lpVtblSimple : NULL;
	return e ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE range_text(ITextRangeProvider *p, int max, BSTR *out) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!out) return E_POINTER;
	*out = NULL;
	if (max < -1) return E_INVALIDARG;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	uint32_t n = r->end - r->start;
	if (max >= 0 && n > ( uint32_t ) max) n = ( uint32_t ) max;
	*out = SysAllocStringLen(doc.len ? doc.wide.text + r->start : L"", n);
	flux_uia_doc_close(&doc);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static uint32_t range_step_forward(UiaDoc const *doc, int unit, uint32_t pos, int count, int *moved) {
	for (*moved = 0; *moved < count; ++*moved) {
		uint32_t next = flux_uia_doc_next(doc, unit, pos);
		if (next >= doc->len || next == pos) break;
		pos = next;
	}
	return pos;
}

static uint32_t range_step_back(UiaDoc const *doc, int unit, uint32_t pos, int count, int *moved) {
	for (*moved = 0; *moved < count && pos > 0; ++*moved) pos = flux_uia_doc_prev(doc, unit, pos);
	return pos;
}

static int range_move(FluxUiaRange *r, UiaDoc const *doc, int unit, int count) {
	uint32_t s = 0, e = 0;
	int      moved = 0;
	if (count == 0 || unit == TextUnit_Page || unit == TextUnit_Document) return 0;
	flux_uia_doc_expand(doc, unit, r->start, &s, &e);
	s = count > 0 ? range_step_forward(doc, unit, s, count, &moved) : range_step_back(doc, unit, s, -count, &moved);
	if (moved == 0) return 0;
	flux_uia_doc_expand(doc, unit, s, &r->start, &r->end);
	return count > 0 ? moved : -moved;
}

static HRESULT STDMETHODCALLTYPE range_move_unit(ITextRangeProvider *p, enum TextUnit unit, int count, int *out) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	if (!out) return E_POINTER;
	*out = 0;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	*out = range_move(r, &doc, unit, count);
	flux_uia_doc_close(&doc);
	return S_OK;
}

static void range_set(FluxUiaRange *r, enum TextPatternRangeEndpoint ep, uint32_t pos) {
	if (ep == TextPatternRangeEndpoint_Start) r->start = pos;
	else r->end = pos;
	if (r->start <= r->end) return;
	if (ep == TextPatternRangeEndpoint_Start) r->end = pos;
	else r->start = pos;
}

static uint32_t range_walk(UiaDoc const *doc, int unit, uint32_t pos, int count, int *moved) {
	*moved = 0;
	while (*moved < count) {
		uint32_t next = flux_uia_doc_next(doc, unit, pos);
		if (next == pos) break;
		pos = next;
		++*moved;
	}
	return pos;
}

static HRESULT STDMETHODCALLTYPE range_move_end(
  ITextRangeProvider *p, enum TextPatternRangeEndpoint ep, enum TextUnit unit, int count, int *out
) {
	FluxUiaRange *r = range_self(p);
	UiaDoc        doc;
	int           moved = 0;
	if (!out) return E_POINTER;
	*out = 0;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	uint32_t at = range_point(r, ep);
	at = count >= 0 ? range_walk(&doc, unit, at, count, &moved) : range_step_back(&doc, unit, at, -count, &moved);
	range_set(r, ep, at);
	*out = count >= 0 ? moved : -moved;
	flux_uia_doc_close(&doc);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE range_move_by_range(
  ITextRangeProvider *p, enum TextPatternRangeEndpoint ep, ITextRangeProvider *target, enum TextPatternRangeEndpoint tep
) {
	FluxUiaRange *r = range_self(p), *t = range_unwrap(target);
	if (!t || t->node != r->node || t->sub != r->sub) return E_INVALIDARG;
	range_set(r, ep, range_point(t, tep));
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE range_select(ITextRangeProvider *p) {
	FluxUiaRange *r = range_self(p);
	flux_uia_dirty(&r->info);
	UiaDoc doc;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	bool ok = doc.view.selectable && doc.hooks->select;
	if (ok) doc.hooks->select(r->info.store, r->node, flux_uia_doc_to8(&doc, r->start), flux_uia_doc_to8(&doc, r->end));
	flux_uia_doc_close(&doc);
	return ok ? S_OK : RANGE_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE range_add(ITextRangeProvider *p) {
	( void ) p;
	return RANGE_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE range_scroll(ITextRangeProvider *p, BOOL top) {
	FluxUiaRange *r = range_self(p);
	flux_uia_dirty(&r->info);
	UiaDoc doc;
	( void ) top;
	if (!range_open(r, &doc)) return UIA_E_ELEMENTNOTAVAILABLE;
	if (doc.hooks->reveal)
		doc.hooks->reveal(r->info.store, r->node, flux_uia_doc_to8(&doc, r->start), flux_uia_doc_to8(&doc, r->end));
	else flux_uia_bring_into_view(&r->info, r->node);
	flux_uia_doc_close(&doc);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE range_children(ITextRangeProvider *p, SAFEARRAY **out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static ITextRangeProviderVtbl const g_range_vtbl = {
  range_qi,
  range_addref,
  range_release,
  range_clone,
  range_compare,
  range_compare_ends,
  range_expand,
  range_find_attr,
  range_find_text,
  range_attr,
  range_rects,
  range_enclosing,
  range_text,
  range_move_unit,
  range_move_end,
  range_move_by_range,
  range_select,
  range_add,
  range_add,
  range_scroll,
  range_children,
};
