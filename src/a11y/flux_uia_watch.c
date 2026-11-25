#include "a11y/flux_uia_doc.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "store/flux_node_data.h"
#include "controls/flux_control_uia.h"
#include "controls/flux_module.h"

#include <stdlib.h>
#include <string.h>

#define WATCH_PROP_NAME          30005
#define WATCH_PROP_ENABLED       30010
#define WATCH_PROP_VALUE         30045
#define WATCH_PROP_RANGE         30047
#define WATCH_PROP_EXPAND        30070
#define WATCH_PROP_TOGGLE        30086
#define WATCH_PROP_SELECTED      30079
#define WATCH_PROP_SCROLL_FIRST  30053
#define WATCH_PAT_SCROLL         10004
#define WATCH_PAT_SELECTION      10001
#define WATCH_PAT_VALUE          10002
#define WATCH_PAT_RANGE          10003
#define WATCH_PAT_EXPAND         10005
#define WATCH_PAT_SELECTION_ITEM 10010
#define WATCH_PAT_TOGGLE         10015
#define WATCH_PAT_TEXT           10014
#define WATCH_PAT_TEXT2          10024
#define WATCH_EVT_ADDED          20010
#define WATCH_EVT_REMOVED        20011
#define WATCH_EVT_SELECTED       20012
#define WATCH_EVT_INVALIDATED    20013
#define WATCH_EVT_TEXT_SELECTION 20014
#define WATCH_EVT_TEXT_CHANGED   20015
#define WATCH_EVT_NOTIFICATION   20035
#define WATCH_BULK_LIMIT         20

enum
{
	W_TOGGLE    = 1u << 0,
	W_EXPAND    = 1u << 1,
	W_RANGE     = 1u << 2,
	W_VALUE     = 1u << 3,
	W_SELITEM   = 1u << 4,
	W_SELECTION = 1u << 5,
	W_TEXT      = 1u << 6,
	W_EDIT      = 1u << 7,
	W_SCROLL    = 1u << 8,
};

enum
{
	WATCH_SCROLL_VALUES = 6,
};

typedef struct WatchEntry {
	XentNodeId node;
	XentNodeId container;
	int        sub;
	uint32_t   name;
	uint32_t   value;
	uint32_t   kids;
	uint32_t   text;
	uint32_t   tsel;
	double     range;
	double     scroll [WATCH_SCROLL_VALUES];
	int        toggle;
	int        expand;
	uint8_t    enabled;
	uint8_t    selected;
	uint16_t   has;
} WatchEntry;

typedef struct WatchTable {
	WatchEntry *e;
	uint32_t    cap;
	uint32_t    count;
} WatchTable;

typedef struct WatchChange {
	XentNodeId container;
	XentNodeId node;
	int        sub;
	bool       added;
} WatchChange;

struct FluxUiaWatch {
	WatchTable   prev;
	WatchTable   cur;
	WatchChange *changes;
	uint32_t     nchanges;
	uint32_t     capchanges;
	bool         primed;
};

static uint32_t watch_slot(WatchTable const *t, XentNodeId node, int sub) {
	uint64_t h = (node ^ (( uint64_t ) ( uint32_t ) sub << 40)) * 0x9e3779b97f4a7c15ull;
	return ( uint32_t ) (h >> 32) & (t->cap - 1u);
}

static WatchEntry const *watch_find(WatchTable const *t, XentNodeId node, int sub) {
	if (!t->cap) return NULL;
	for (uint32_t i = watch_slot(t, node, sub);; i = (i + 1u) & (t->cap - 1u)) {
		WatchEntry const *e = &t->e [i];
		if (!e->node) return NULL;
		if (e->node == node && e->sub == sub) return e;
	}
}

static void watch_insert(WatchTable *t, WatchEntry const *src) {
	uint32_t i = watch_slot(t, src->node, src->sub);
	while (t->e [i].node && !(t->e [i].node == src->node && t->e [i].sub == src->sub)) i = (i + 1u) & (t->cap - 1u);
	t->count += !t->e [i].node;
	t->e [i]  = *src;
}

static bool watch_grow(WatchTable *t) {
	if (t->cap && (t->count + 1u) * 2u <= t->cap) return true;
	WatchTable bigger = {calloc(t->cap ? t->cap * 2u : 256u, sizeof(WatchEntry)), t->cap ? t->cap * 2u : 256u, 0u};
	if (!bigger.e) return false;
	for (uint32_t i = 0; i < t->cap; i++)
		if (t->e [i].node) watch_insert(&bigger, &t->e [i]);
	free(t->e);
	*t = bigger;
	return true;
}

static void watch_clear(WatchTable *t) {
	if (t->e) memset(t->e, 0, t->cap * sizeof(WatchEntry));
	t->count = 0u;
}

static uint32_t watch_hash_wide(BSTR s) {
	uint32_t h = 2166136261u;
	for (UINT i = 0, n = s ? SysStringLen(s) : 0u; i < n; i++) h = (h ^ s [i]) * 16777619u;
	return h;
}

static IRawElementProviderSimple *watch_simple(FluxUia *p) { return ( IRawElementProviderSimple * ) &p->lpVtblSimple; }

static uint32_t                   watch_prop_hash(FluxUia *p, PROPERTYID id) {
	VARIANT v;
	VariantInit(&v);
	IRawElementProviderSimple_GetPropertyValue(watch_simple(p), id, &v);
	uint32_t h = v.vt == VT_BSTR ? watch_hash_wide(v.bstrVal) : 0u;
	VariantClear(&v);
	return h;
}

static bool watch_prop_bool(FluxUia *p, PROPERTYID id) {
	VARIANT v;
	VariantInit(&v);
	IRawElementProviderSimple_GetPropertyValue(watch_simple(p), id, &v);
	bool b = v.vt == VT_BOOL && v.boolVal == VARIANT_TRUE;
	VariantClear(&v);
	return b;
}

static IUnknown *watch_pattern(FluxUia *p, PATTERNID id) {
	IUnknown *u = NULL;
	IRawElementProviderSimple_GetPatternProvider(watch_simple(p), id, &u);
	return u;
}

static void watch_read_toggle(FluxUia *p, WatchEntry *e) {
	IToggleProvider *t = ( IToggleProvider * ) watch_pattern(p, WATCH_PAT_TOGGLE);
	if (!t) return;
	enum ToggleState s = ToggleState_Off;
	IToggleProvider_get_ToggleState(t, &s);
	e->toggle  = ( int ) s;
	e->has    |= W_TOGGLE;
	IToggleProvider_Release(t);
}

static void watch_read_expand(FluxUia *p, WatchEntry *e) {
	IExpandCollapseProvider *x = ( IExpandCollapseProvider * ) watch_pattern(p, WATCH_PAT_EXPAND);
	if (!x) return;
	enum ExpandCollapseState s = ExpandCollapseState_Collapsed;
	IExpandCollapseProvider_get_ExpandCollapseState(x, &s);
	e->expand  = ( int ) s;
	e->has    |= W_EXPAND;
	IExpandCollapseProvider_Release(x);
}

static void watch_read_range(FluxUia *p, WatchEntry *e) {
	IRangeValueProvider *r = ( IRangeValueProvider * ) watch_pattern(p, WATCH_PAT_RANGE);
	if (!r) return;
	IRangeValueProvider_get_Value(r, &e->range);
	e->has |= W_RANGE;
	IRangeValueProvider_Release(r);
}

static void watch_read_value(FluxUia *p, WatchEntry *e) {
	IValueProvider *v = ( IValueProvider * ) watch_pattern(p, WATCH_PAT_VALUE);
	if (!v) return;
	BSTR s = NULL;
	IValueProvider_get_Value(v, &s);
	e->value  = watch_hash_wide(s);
	e->has   |= W_VALUE;
	SysFreeString(s);
	IValueProvider_Release(v);
}

static void watch_read_selection(FluxUia *p, WatchEntry *e) {
	ISelectionItemProvider *s = ( ISelectionItemProvider * ) watch_pattern(p, WATCH_PAT_SELECTION_ITEM);
	IUnknown               *c = watch_pattern(p, WATCH_PAT_SELECTION);
	if (c) {
		e->has |= W_SELECTION;
		c->lpVtbl->Release(c);
	}
	if (!s) return;
	BOOL on = FALSE;
	ISelectionItemProvider_get_IsSelected(s, &on);
	e->selected  = on ? 1u : 0u;
	e->has      |= W_SELITEM;
	ISelectionItemProvider_Release(s);
}

static void watch_read_scroll(FluxUia *p, WatchEntry *e) {
	IScrollProvider *s = ( IScrollProvider * ) watch_pattern(p, WATCH_PAT_SCROLL);
	BOOL             h = FALSE, v = FALSE;
	if (!s) return;
	IScrollProvider_get_HorizontalScrollPercent(s, &e->scroll [0]);
	IScrollProvider_get_HorizontalViewSize(s, &e->scroll [1]);
	IScrollProvider_get_VerticalScrollPercent(s, &e->scroll [2]);
	IScrollProvider_get_VerticalViewSize(s, &e->scroll [3]);
	IScrollProvider_get_HorizontallyScrollable(s, &h);
	IScrollProvider_get_VerticallyScrollable(s, &v);
	e->scroll [4]  = h ? 1.0 : 0.0;
	e->scroll [5]  = v ? 1.0 : 0.0;
	e->has        |= W_SCROLL;
	IScrollProvider_Release(s);
}

static bool watch_has_pattern(FluxUia *p, PATTERNID id) {
	IUnknown *u = watch_pattern(p, id);
	if (u) u->lpVtbl->Release(u);
	return u != NULL;
}

static void watch_read_text(FluxUia *p, WatchEntry *e) {
	if (!watch_has_pattern(p, WATCH_PAT_TEXT)) return;
	if (!flux_uia_doc_state(&p->info, p->node, &e->text, &e->tsel)) return;
	e->has |= watch_has_pattern(p, WATCH_PAT_TEXT2) ? W_TEXT | W_EDIT : W_TEXT;
}

static void watch_read(FluxUia *p, WatchEntry *e) {
	e->name    = watch_prop_hash(p, WATCH_PROP_NAME);
	e->enabled = watch_prop_bool(p, WATCH_PROP_ENABLED);
	watch_read_toggle(p, e);
	watch_read_expand(p, e);
	watch_read_range(p, e);
	watch_read_value(p, e);
	watch_read_selection(p, e);
	watch_read_text(p, e);
	watch_read_scroll(p, e);
}

static void watch_raise_variant(FluxUia *p, PROPERTYID id, VARIANT old_v, VARIANT new_v) {
	UiaRaiseAutomationPropertyChangedEvent(watch_simple(p), id, old_v, new_v);
}

static void watch_raise_i4(FluxUia *p, PROPERTYID id, int old_i, int new_i) {
	VARIANT o = {.vt = VT_I4}, n = {.vt = VT_I4};
	o.lVal = old_i;
	n.lVal = new_i;
	watch_raise_variant(p, id, o, n);
}

static void watch_raise_current(FluxUia *p, PROPERTYID id) {
	VARIANT o, n;
	VariantInit(&o);
	VariantInit(&n);
	IRawElementProviderSimple_GetPropertyValue(watch_simple(p), id, &n);
	watch_raise_variant(p, id, o, n);
	VariantClear(&n);
}

static void watch_raise_value(FluxUia *p) {
	IValueProvider *v = ( IValueProvider * ) watch_pattern(p, WATCH_PAT_VALUE);
	if (!v) return;
	VARIANT o, n;
	VariantInit(&o);
	VariantInit(&n);
	if (SUCCEEDED(IValueProvider_get_Value(v, &n.bstrVal))) n.vt = VT_BSTR;
	watch_raise_variant(p, WATCH_PROP_VALUE, o, n);
	VariantClear(&n);
	IValueProvider_Release(v);
}

static void watch_raise_range_id(FluxUia *p, PROPERTYID id, double old_d, double new_d) {
	VARIANT o = {.vt = VT_R8}, n = {.vt = VT_R8};
	o.dblVal = old_d;
	n.dblVal = new_d;
	watch_raise_variant(p, id, o, n);
}

static void watch_raise_range(FluxUia *p, double old_d, double new_d) {
	watch_raise_range_id(p, WATCH_PROP_RANGE, old_d, new_d);
}

static bool watch_both(WatchEntry const *a, WatchEntry const *b, unsigned bit) { return (a->has & b->has & bit) != 0u; }

static void watch_raise_bool(FluxUia *p, PROPERTYID id, bool old_b, bool new_b) {
	VARIANT o = {.vt = VT_BOOL}, n = {.vt = VT_BOOL};
	o.boolVal = old_b ? VARIANT_TRUE : VARIANT_FALSE;
	n.boolVal = new_b ? VARIANT_TRUE : VARIANT_FALSE;
	watch_raise_variant(p, id, o, n);
}

static void watch_raise_scroll(FluxUia *p, WatchEntry const *o, WatchEntry const *n) {
	static PROPERTYID const ids [WATCH_SCROLL_VALUES] = {
	  WATCH_PROP_SCROLL_FIRST, WATCH_PROP_SCROLL_FIRST + 1, WATCH_PROP_SCROLL_FIRST + 2, WATCH_PROP_SCROLL_FIRST + 3,
	  WATCH_PROP_SCROLL_FIRST + 4, WATCH_PROP_SCROLL_FIRST + 5};
	if (!watch_both(o, n, W_SCROLL)) return;
	for (int i = 0; i < 4; i++)
		if (o->scroll [i] != n->scroll [i]) watch_raise_range_id(p, ids [i], o->scroll [i], n->scroll [i]);
	for (int i = 4; i < WATCH_SCROLL_VALUES; i++)
		if (o->scroll [i] != n->scroll [i]) watch_raise_bool(p, ids [i], o->scroll [i] != 0.0, n->scroll [i] != 0.0);
}

static void watch_raise_props(FluxUia *p, WatchEntry const *o, WatchEntry const *n) {
	if (o->name != n->name) watch_raise_current(p, WATCH_PROP_NAME);
	if (o->enabled != n->enabled) watch_raise_current(p, WATCH_PROP_ENABLED);
	if (watch_both(o, n, W_TOGGLE) && o->toggle != n->toggle)
		watch_raise_i4(p, WATCH_PROP_TOGGLE, o->toggle, n->toggle);
	if (watch_both(o, n, W_EXPAND) && o->expand != n->expand)
		watch_raise_i4(p, WATCH_PROP_EXPAND, o->expand, n->expand);
	if (watch_both(o, n, W_RANGE) && o->range != n->range) watch_raise_range(p, o->range, n->range);
	if (watch_both(o, n, W_VALUE) && o->value != n->value) watch_raise_value(p);
}

static void watch_raise_text(FluxUia *p, WatchEntry const *o, WatchEntry const *n) {
	if (watch_both(o, n, W_EDIT) && o->text != n->text)
		UiaRaiseAutomationEvent(watch_simple(p), WATCH_EVT_TEXT_CHANGED);
	if (watch_both(o, n, W_TEXT) && o->tsel != n->tsel)
		UiaRaiseAutomationEvent(watch_simple(p), WATCH_EVT_TEXT_SELECTION);
}

static void watch_raise_structure(FluxUia *p) {
	SAFEARRAY *sa = NULL;
	IRawElementProviderFragment_GetRuntimeId(( IRawElementProviderFragment * ) &p->lpVtblFragment, &sa);
	int *ids = NULL;
	if (!sa || FAILED(SafeArrayAccessData(sa, ( void ** ) &ids))) return;
	int n = ( int ) sa->rgsabound [0].cElements;
	UiaRaiseStructureChangedEvent(watch_simple(p), StructureChangeType_ChildrenInvalidated, ids, n);
	SafeArrayUnaccessData(sa);
	SafeArrayDestroy(sa);
}

static void watch_note_change(FluxUiaWatch *w, WatchEntry const *n) {
	if (w->nchanges == w->capchanges) {
		uint32_t     cap  = w->capchanges ? w->capchanges * 2u : 32u;
		WatchChange *next = ( WatchChange * ) realloc(w->changes, cap * sizeof(*next));
		if (!next) return;
		w->changes    = next;
		w->capchanges = cap;
	}
	w->changes [w->nchanges++] = (WatchChange) {n->container, n->node, n->sub, n->selected != 0u};
}

static bool watch_per_item(FluxUiaContext const *info, XentNodeId container) {
	FluxUiaHooks const *h = container ? flux_module_uia(info->store, container) : NULL;
	return h && h->select_mode && (h->select_mode(info->store, container) & FLUX_UIA_SEL_EACH);
}

static void watch_compare_selection(
  FluxUiaWatch *w, FluxUiaContext const *info, FluxUia *p, WatchEntry const *old, WatchEntry const *now
) {
	if (!watch_both(old, now, W_SELITEM) || old->selected == now->selected) return;
	bool selected = now->selected != 0u;
	if (watch_per_item(info, now->container))
		UiaRaiseAutomationEvent(watch_simple(p), selected ? WATCH_EVT_ADDED : WATCH_EVT_REMOVED);
	else watch_note_change(w, now);
	watch_raise_bool(p, WATCH_PROP_SELECTED, !selected, selected);
}

static void watch_compare(FluxUiaWatch *w, FluxUiaContext const *info, FluxUia *p, WatchEntry const *now) {
	WatchEntry const *old = w->primed ? watch_find(&w->prev, now->node, now->sub) : NULL;
	if (!old) return;
	watch_raise_props(p, old, now);
	watch_raise_text(p, old, now);
	watch_raise_scroll(p, old, now);
	watch_compare_selection(w, info, p, old, now);
	if (now->sub < 0 && old->kids != now->kids) watch_raise_structure(p);
}

static uint32_t
watch_visit(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, int sub, XentNodeId container);

static uint32_t watch_children(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, XentNodeId container) {
	uint32_t h     = 2166136261u;
	int      first = 0, last = -1;
	if (flux_uia_sub_range(info, node, &first, &last)) {
		for (int i = first; i <= last; i++) h = (h ^ watch_visit(w, info, node, i, container)) * 16777619u;
		return h;
	}
	for (XentNodeId c = flux_uia_first(info, node); c != XENT_NODE_INVALID; c = flux_uia_next(info, c))
		h = (h ^ watch_visit(w, info, c, -1, container)) * 16777619u;
	return h;
}

static uint32_t
watch_visit(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, int sub, XentNodeId container) {
	FluxUia *p = sub < 0 ? flux_uia_alloc(info, node) : flux_uia_alloc_sub(info, node, sub);
	if (!p) return 0u;
	WatchEntry now = {.node = node, .container = container, .sub = sub};
	watch_read(p, &now);
	if (sub < 0) now.kids = watch_children(w, info, node, (now.has & W_SELECTION) ? node : container);
	watch_compare(w, info, p, &now);
	if (watch_grow(&w->cur)) watch_insert(&w->cur, &now);
	flux_uia_release(p);
	return ( uint32_t ) (node * 2654435761u) ^ ( uint32_t ) (sub + 1);
}

static void watch_raise_item(FluxUiaContext const *info, WatchChange const *c, int event_id) {
	FluxUia *p = c->sub < 0 ? flux_uia_alloc(info, c->node) : flux_uia_alloc_sub(info, c->node, c->sub);
	if (!p) return;
	UiaRaiseAutomationEvent(watch_simple(p), event_id);
	flux_uia_release(p);
}

static uint32_t watch_selected_in(WatchTable const *t, XentNodeId container) {
	uint32_t n = 0u;
	for (uint32_t i = 0; i < t->cap; i++) n += t->e [i].node && t->e [i].container == container && t->e [i].selected;
	return n;
}

static WatchChange const *watch_first_of(FluxUiaWatch const *w, XentNodeId container, bool added) {
	for (uint32_t i = 0; i < w->nchanges; i++)
		if (w->changes [i].container == container && w->changes [i].added == added) return &w->changes [i];
	return NULL;
}

static void watch_count(FluxUiaWatch const *w, XentNodeId container, uint32_t *added, uint32_t *removed) {
	*added = *removed = 0u;
	for (uint32_t i = 0; i < w->nchanges; i++)
		if (w->changes [i].container == container) *(w->changes [i].added ? added : removed) += 1u;
}

static void watch_each_item(FluxUiaWatch const *w, FluxUiaContext const *info, XentNodeId container) {
	for (uint32_t i = 0; i < w->nchanges; i++)
		if (w->changes [i].container == container)
			watch_raise_item(info, &w->changes [i], w->changes [i].added ? WATCH_EVT_ADDED : WATCH_EVT_REMOVED);
}

static void watch_invalidate(FluxUiaContext const *info, XentNodeId container) {
	FluxUia *p = container ? flux_uia_alloc(info, container) : NULL;
	if (!p) return;
	UiaRaiseAutomationEvent(watch_simple(p), WATCH_EVT_INVALIDATED);
	flux_uia_release(p);
}

static void watch_selection_of(FluxUiaWatch const *w, FluxUiaContext const *info, XentNodeId container) {
	uint32_t added = 0u, removed = 0u;
	watch_count(w, container, &added, &removed);
	if (added == 1u && watch_selected_in(&w->cur, container) == 1u) {
		watch_raise_item(info, watch_first_of(w, container, true), WATCH_EVT_SELECTED);
		if (removed == 1u) watch_raise_item(info, watch_first_of(w, container, false), WATCH_EVT_REMOVED);
		return;
	}
	if (added + removed > WATCH_BULK_LIMIT) watch_invalidate(info, container);
	else watch_each_item(w, info, container);
}

static bool watch_seen_before(FluxUiaWatch const *w, uint32_t i) {
	for (uint32_t k = 0; k < i; k++)
		if (w->changes [k].container == w->changes [i].container) return true;
	return false;
}

static void watch_selection(FluxUiaWatch *w, FluxUiaContext const *info) {
	for (uint32_t i = 0; i < w->nchanges; i++)
		if (!watch_seen_before(w, i)) watch_selection_of(w, info, w->changes [i].container);
	w->nchanges = 0u;
}

static BSTR watch_bstr(char const *utf8) {
	int  n = utf8 ? MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0) : 0;
	BSTR s = n > 0 ? SysAllocStringLen(NULL, ( UINT ) (n - 1)) : NULL;
	if (s) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, s, n);
	return s;
}

static void watch_raise_notification(IRawElementProviderSimple *provider, FluxUiaNote const *n) {
	BSTR text = watch_bstr(n->text), activity = watch_bstr(n->activity);
	if (!activity) activity = SysAllocString(L"");
	UiaRaiseNotificationEvent(
	  provider, ( enum NotificationKind ) n->kind, ( enum NotificationProcessing ) n->processing, text, activity
	);
	SysFreeString(text);
	SysFreeString(activity);
}

static void watch_raise_note(FluxUiaContext const *info, FluxUiaNote const *n) {
	if (!xent_node_valid(info->ctx, n->node)) return;
	FluxUia *p = n->sub < 0 ? flux_uia_alloc(info, n->node) : flux_uia_alloc_sub(info, n->node, n->sub);
	if (!p) return;
	if (n->event < 0) {
		VARIANT o = {.vt = VT_R8}, v = {.vt = VT_R8};
		o.dblVal = n->old_v;
		v.dblVal = n->new_v;
		UiaRaiseAutomationPropertyChangedEvent(watch_simple(p), -n->event, o, v);
	}
	else if (n->event == WATCH_EVT_NOTIFICATION) watch_raise_notification(watch_simple(p), n);
	else UiaRaiseAutomationEvent(watch_simple(p), n->event);
	flux_uia_release(p);
}

static void watch_notes(FluxUiaContext const *info, bool listening) {
	for (uint32_t i = 0; listening && i < flux_store_nnotes(info->store); i++)
		watch_raise_note(info, flux_store_note(info->store, i));
	flux_store_clearnotes(info->store);
}

static void watch_reset(FluxUiaWatch *w) {
	watch_clear(&w->prev);
	watch_clear(&w->cur);
	w->nchanges = 0u;
	w->primed   = false;
}

FluxUiaWatch *flux_uia_watch_create(void) { return ( FluxUiaWatch * ) calloc(1, sizeof(FluxUiaWatch)); }

void          flux_uia_watch_destroy(FluxUiaWatch *w) {
	if (!w) return;
	free(w->prev.e);
	free(w->cur.e);
	free(w->changes);
	free(w);
}

void flux_uia_watch_flush(FluxUiaContext const *info) {
	if (!info || !info->ctx || !info->store) return;
	watch_notes(info, UiaClientsAreListening());
}

void flux_uia_watch_pump(FluxUiaWatch *w, FluxUiaContext const *info) {
	if (!w || !info || !info->ctx || info->root == XENT_NODE_INVALID) return;
	bool listening = UiaClientsAreListening();
	watch_notes(info, listening);
	if (!listening) {
		if (w->primed) watch_reset(w);
		return;
	}
	watch_visit(w, info, info->root, -1, XENT_NODE_INVALID);
	watch_selection(w, info);
	WatchTable done = w->prev;
	w->prev         = w->cur;
	w->cur          = done;
	watch_clear(&w->cur);
	w->primed = true;
}
