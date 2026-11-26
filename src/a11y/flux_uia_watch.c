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
	WATCH_EMPTY         = 0,
	WATCH_OCCUPIED      = 1,
	WATCH_TOMBSTONE     = 2,
};

typedef struct WatchEntry {
	XentNodeId node;
	XentNodeId container;
	int        sub;
	uint32_t   stamp;
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
	uint8_t    slot_state;
	uint16_t   has;
} WatchEntry;

typedef struct WatchTable {
	WatchEntry *e;
	uint32_t    cap;
	uint32_t    count;
	uint32_t    tombstones;
} WatchTable;

typedef struct WatchChange {
	XentNodeId container;
	XentNodeId node;
	int        sub;
	bool       added;
} WatchChange;

struct FluxUiaWatch {
	WatchTable        table;
	WatchChange      *changes;
	uint32_t          nchanges;
	uint32_t          capchanges;
	FluxUiaDirtyItem *dirty_buf;
	uint32_t          dirty_cap;
	XentNodeId       *sel_containers;
	uint32_t          nsel_containers;
	uint32_t          cap_sel_containers;
	uint32_t          pump_gen;
	bool              primed;
};

static uint32_t watch_slot(WatchTable const *t, XentNodeId node, int sub) {
	uint64_t h = (node ^ (( uint64_t ) ( uint32_t ) sub << 40)) * 0x9e3779b97f4a7c15ull;
	return ( uint32_t ) (h >> 32) & (t->cap - 1u);
}

static WatchEntry *watch_find(WatchTable const *t, XentNodeId node, int sub) {
	if (!t->cap) return NULL;
	for (uint32_t i = watch_slot(t, node, sub);; i = (i + 1u) & (t->cap - 1u)) {
		WatchEntry *e = &t->e [i];
		if (e->slot_state == WATCH_EMPTY) return NULL;
		if (e->slot_state == WATCH_OCCUPIED && e->node == node && e->sub == sub) return e;
	}
}

static void watch_insert_raw(WatchTable *t, WatchEntry const *src) {
	uint32_t dead = UINT32_MAX;
	uint32_t i    = watch_slot(t, src->node, src->sub);
	for (; t->e [i].slot_state != WATCH_EMPTY; i = (i + 1u) & (t->cap - 1u)) {
		WatchEntry *e = &t->e [i];
		if (e->slot_state == WATCH_TOMBSTONE && dead == UINT32_MAX) dead = i;
		if (e->slot_state == WATCH_OCCUPIED && e->node == src->node && e->sub == src->sub) {
			*e            = *src;
			e->slot_state = WATCH_OCCUPIED;
			return;
		}
	}
	uint32_t dst = dead != UINT32_MAX ? dead : i;
	if (t->e [dst].slot_state == WATCH_TOMBSTONE) t->tombstones--;
	t->count++;
	t->e [dst]            = *src;
	t->e [dst].slot_state = WATCH_OCCUPIED;
}

static bool watch_grow(WatchTable *t) {
	if (t->cap && (t->count + t->tombstones + 1u) * 2u <= t->cap) return true;
	uint32_t   next_cap = !t->cap ? 256u : ((t->count + 1u) * 4u <= t->cap ? t->cap : t->cap * 2u);
	WatchTable bigger   = {calloc(next_cap, sizeof(WatchEntry)), next_cap, 0u, 0u};
	if (!bigger.e) return false;
	for (uint32_t i = 0u; i < t->cap; i++)
		if (t->e [i].slot_state == WATCH_OCCUPIED) watch_insert_raw(&bigger, &t->e [i]);
	free(t->e);
	*t = bigger;
	return true;
}

static void watch_remove_slot(WatchTable *t, uint32_t i) {
	t->e [i].slot_state = WATCH_TOMBSTONE;
	t->e [i].node       = XENT_NODE_INVALID;
	t->count--;
	t->tombstones++;
}

static void watch_remove_node_all(WatchTable *t, XentNodeId node) {
	for (uint32_t i = 0u; i < t->cap; i++)
		if (t->e [i].slot_state == WATCH_OCCUPIED && t->e [i].node == node) watch_remove_slot(t, i);
}

static void watch_remove_stale_subs(WatchTable *t, XentNodeId node, bool has_subs, int first, int last) {
	for (uint32_t i = 0u; i < t->cap; i++) {
		WatchEntry const *e = &t->e [i];
		if (e->slot_state != WATCH_OCCUPIED || e->node != node || e->sub < 0) continue;
		if (!has_subs || e->sub < first || e->sub > last) watch_remove_slot(t, i);
	}
}

static void watch_clear(WatchTable *t) {
	if (t->e && t->cap) memset(t->e, 0, t->cap * sizeof(WatchEntry));
	t->count      = 0u;
	t->tombstones = 0u;
}

static uint32_t watch_hash_utf8(char const *s) {
	uint32_t h = 2166136261u;
	if (!s || !s [0]) return h;
	for (unsigned char const *p = ( unsigned char const * ) s; *p; p++) h = (h ^ *p) * 16777619u;
	return h;
}

static IRawElementProviderSimple *watch_simple(FluxUia *p) { return ( IRawElementProviderSimple * ) &p->lpVtblSimple; }

static FluxUia                   *watch_live(FluxUiaContext const *info, WatchEntry const *n, FluxUia **slot) {
	if (!*slot) *slot = n->sub < 0 ? flux_uia_alloc(info, n->node) : flux_uia_alloc_sub(info, n->node, n->sub);
	return *slot;
}

static IUnknown *watch_pattern(FluxUia *p, PATTERNID id) {
	IUnknown *u = NULL;
	IRawElementProviderSimple_GetPatternProvider(watch_simple(p), id, &u);
	return u;
}

static bool watch_has_pattern(FluxUia *p, PATTERNID id) {
	IUnknown *u = watch_pattern(p, id);
	if (u) u->lpVtbl->Release(u);
	return u != NULL;
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
	if (!watch_has_pattern(p, WATCH_PAT_VALUE)) return;
	e->value  = watch_hash_utf8(flux_uia_value_utf8(p));
	e->has   |= W_VALUE;
}

static void watch_read_selection(FluxUia *p, WatchEntry *e) {
	ISelectionItemProvider *s = ( ISelectionItemProvider * ) watch_pattern(p, WATCH_PAT_SELECTION_ITEM);
	if (watch_has_pattern(p, WATCH_PAT_SELECTION)) e->has |= W_SELECTION;
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

static void watch_read_text(FluxUia *p, WatchEntry *e) {
	if (!watch_has_pattern(p, WATCH_PAT_TEXT)) return;
	if (!flux_uia_doc_state(&p->info, p->node, &e->text, &e->tsel)) return;
	e->has |= watch_has_pattern(p, WATCH_PAT_TEXT2) ? W_TEXT | W_EDIT : W_TEXT;
}

static uint32_t watch_entry_tag(XentNodeId node, int sub) {
	return ( uint32_t ) (node * 2654435761u) ^ ( uint32_t ) (sub + 1);
}

static uint32_t watch_kids_hash(FluxUiaContext const *info, XentNodeId node) {
	uint32_t h     = 2166136261u;
	int      first = 0, last = -1;
	if (flux_uia_sub_range(info, node, &first, &last)) {
		for (int i = first; i <= last; i++) h = (h ^ watch_entry_tag(node, i)) * 16777619u;
		return h;
	}
	for (XentNodeId c = flux_uia_first(info, node); c != XENT_NODE_INVALID; c = flux_uia_next(info, c))
		h = (h ^ watch_entry_tag(c, -1)) * 16777619u;
	return h;
}

static void watch_read(FluxUia *p, WatchEntry *e) {
	char buf [256] = {0};
	e->name        = watch_hash_utf8(flux_uia_name_utf8(p, buf, sizeof(buf)));
	e->enabled     = flux_uia_enabled_bool(p) ? 1u : 0u;
	watch_read_toggle(p, e);
	watch_read_expand(p, e);
	watch_read_range(p, e);
	watch_read_value(p, e);
	watch_read_selection(p, e);
	watch_read_text(p, e);
	watch_read_scroll(p, e);
	if (e->sub < 0) e->kids = watch_kids_hash(&p->info, e->node);
}

static void watch_raise_variant(FluxUia *p, PROPERTYID id, VARIANT old_v, VARIANT new_v) {
	if (p) UiaRaiseAutomationPropertyChangedEvent(watch_simple(p), id, old_v, new_v);
}

static void watch_raise_i4(FluxUia *p, PROPERTYID id, int old_i, int new_i) {
	VARIANT o = {.vt = VT_I4}, n = {.vt = VT_I4};
	o.lVal = old_i;
	n.lVal = new_i;
	watch_raise_variant(p, id, o, n);
}

static void watch_raise_current(FluxUia *p, PROPERTYID id) {
	if (!p) return;
	VARIANT o, n;
	VariantInit(&o);
	VariantInit(&n);
	IRawElementProviderSimple_GetPropertyValue(watch_simple(p), id, &n);
	watch_raise_variant(p, id, o, n);
	VariantClear(&n);
}

static void watch_raise_value(FluxUia *p) {
	IValueProvider *v = p ? ( IValueProvider * ) watch_pattern(p, WATCH_PAT_VALUE) : NULL;
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

static void watch_raise_bool(FluxUia *p, PROPERTYID id, bool old_b, bool new_b) {
	VARIANT o = {.vt = VT_BOOL}, n = {.vt = VT_BOOL};
	o.boolVal = old_b ? VARIANT_TRUE : VARIANT_FALSE;
	n.boolVal = new_b ? VARIANT_TRUE : VARIANT_FALSE;
	watch_raise_variant(p, id, o, n);
}

static bool watch_both(WatchEntry const *a, WatchEntry const *b, unsigned bit) { return (a->has & b->has & bit) != 0u; }

static void watch_raise_scroll(FluxUiaContext const *info, WatchEntry const *o, WatchEntry const *n, FluxUia **live) {
	static PROPERTYID const ids [WATCH_SCROLL_VALUES] = {
	  WATCH_PROP_SCROLL_FIRST, WATCH_PROP_SCROLL_FIRST + 1, WATCH_PROP_SCROLL_FIRST + 2, WATCH_PROP_SCROLL_FIRST + 3,
	  WATCH_PROP_SCROLL_FIRST + 4, WATCH_PROP_SCROLL_FIRST + 5};
	if (!watch_both(o, n, W_SCROLL)) return;
	for (int i = 0; i < 4; i++)
		if (o->scroll [i] != n->scroll [i])
			watch_raise_range_id(watch_live(info, n, live), ids [i], o->scroll [i], n->scroll [i]);
	for (int i = 4; i < WATCH_SCROLL_VALUES; i++)
		if (o->scroll [i] != n->scroll [i])
			watch_raise_bool(watch_live(info, n, live), ids [i], o->scroll [i] != 0.0, n->scroll [i] != 0.0);
}

static void
watch_raise_scalar_props(FluxUiaContext const *info, WatchEntry const *o, WatchEntry const *n, FluxUia **live) {
	if (o->name != n->name) watch_raise_current(watch_live(info, n, live), WATCH_PROP_NAME);
	if (o->enabled != n->enabled) watch_raise_current(watch_live(info, n, live), WATCH_PROP_ENABLED);
	if (watch_both(o, n, W_VALUE) && o->value != n->value) watch_raise_value(watch_live(info, n, live));
}

static void watch_raise_props(FluxUiaContext const *info, WatchEntry const *o, WatchEntry const *n, FluxUia **live) {
	watch_raise_scalar_props(info, o, n, live);
	if (watch_both(o, n, W_TOGGLE) && o->toggle != n->toggle)
		watch_raise_i4(watch_live(info, n, live), WATCH_PROP_TOGGLE, o->toggle, n->toggle);
	if (watch_both(o, n, W_EXPAND) && o->expand != n->expand)
		watch_raise_i4(watch_live(info, n, live), WATCH_PROP_EXPAND, o->expand, n->expand);
	if (watch_both(o, n, W_RANGE) && o->range != n->range)
		watch_raise_range_id(watch_live(info, n, live), WATCH_PROP_RANGE, o->range, n->range);
}

static void watch_raise_text(FluxUiaContext const *info, WatchEntry const *o, WatchEntry const *n, FluxUia **live) {
	if (watch_both(o, n, W_EDIT) && o->text != n->text) {
		FluxUia *p = watch_live(info, n, live);
		if (p) UiaRaiseAutomationEvent(watch_simple(p), WATCH_EVT_TEXT_CHANGED);
	}
	if (watch_both(o, n, W_TEXT) && o->tsel != n->tsel) {
		FluxUia *p = watch_live(info, n, live);
		if (p) UiaRaiseAutomationEvent(watch_simple(p), WATCH_EVT_TEXT_SELECTION);
	}
}

static void watch_raise_structure(FluxUia *p) {
	if (!p) return;
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

static void watch_push_sel_container(FluxUiaWatch *w, XentNodeId c) {
	if (c == XENT_NODE_INVALID) return;
	for (uint32_t i = 0u; i < w->nsel_containers; i++)
		if (w->sel_containers [i] == c) return;
	if (w->nsel_containers == w->cap_sel_containers) {
		uint32_t    cap  = w->cap_sel_containers ? w->cap_sel_containers * 2u : 16u;
		XentNodeId *next = ( XentNodeId * ) realloc(w->sel_containers, cap * sizeof(*next));
		if (!next) return;
		w->sel_containers     = next;
		w->cap_sel_containers = cap;
	}
	w->sel_containers [w->nsel_containers++] = c;
}

static bool watch_per_item(FluxUiaContext const *info, XentNodeId container) {
	FluxUiaHooks const *h = container ? flux_module_uia(info->store, container) : NULL;
	return h && h->select_mode && (h->select_mode(info->store, container) & FLUX_UIA_SEL_EACH);
}

static void watch_compare_selection(
  FluxUiaWatch *w, FluxUiaContext const *info, WatchEntry const *old, WatchEntry const *now, FluxUia **live
) {
	if (!watch_both(old, now, W_SELITEM) || old->selected == now->selected) return;
	bool     selected = now->selected != 0u;
	FluxUia *p        = watch_live(info, now, live);
	if (watch_per_item(info, now->container)) {
		if (p) UiaRaiseAutomationEvent(watch_simple(p), selected ? WATCH_EVT_ADDED : WATCH_EVT_REMOVED);
	}
	else watch_note_change(w, now);
	watch_raise_bool(p, WATCH_PROP_SELECTED, !selected, selected);
}

static void watch_compare(FluxUiaWatch *w, FluxUiaContext const *info, WatchEntry const *old, WatchEntry const *now) {
	if (!w->primed || !old) return;
	FluxUia *live = NULL;
	watch_raise_props(info, old, now, &live);
	watch_raise_text(info, old, now, &live);
	watch_raise_scroll(info, old, now, &live);
	watch_compare_selection(w, info, old, now, &live);
	if (now->sub < 0 && old->kids != now->kids) watch_raise_structure(watch_live(info, now, &live));
	if (live) flux_uia_release(live);
}

static void watch_fill_entry(
  FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, int sub, XentNodeId container, WatchEntry *now
) {
	FluxUia probe;
	memset(&probe, 0, sizeof(probe));
	flux_uia_init(&probe, info, node, sub);
	*now = (WatchEntry) {.node = node, .container = container, .sub = sub, .stamp = w->pump_gen};
	watch_read(&probe, now);
}

static void watch_track_sel(FluxUiaWatch *w, WatchEntry const *old, WatchEntry const *now) {
	if (now->has & W_SELECTION) watch_push_sel_container(w, now->node);
	if (now->has & W_SELITEM) watch_push_sel_container(w, now->container);
	if (old && (old->has & W_SELITEM)) watch_push_sel_container(w, old->container);
}

static void watch_walk_tree(
  FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, int sub, XentNodeId container, bool emit
) {
	WatchEntry now;
	watch_fill_entry(w, info, node, sub, container, &now);
	WatchEntry const *old = emit ? watch_find(&w->table, node, sub) : NULL;
	if (emit) watch_compare(w, info, old, &now);
	if (watch_grow(&w->table)) watch_insert_raw(&w->table, &now);
	if (sub >= 0) return;

	XentNodeId child_container = (now.has & W_SELECTION) ? node : container;
	int        first = 0, last = -1;
	if (flux_uia_sub_range(info, node, &first, &last)) {
		for (int i = first; i <= last; i++) watch_walk_tree(w, info, node, i, child_container, emit);
		return;
	}
	for (XentNodeId c = flux_uia_first(info, node); c != XENT_NODE_INVALID; c = flux_uia_next(info, c))
		watch_walk_tree(w, info, c, -1, child_container, emit);
}

static XentNodeId watch_find_container(FluxUiaWatch const *w, FluxUiaContext const *info, XentNodeId node, int sub) {
	if (sub >= 0) {
		WatchEntry const *p = watch_find(&w->table, node, -1);
		bool is_sel = p ? (p->has & W_SELECTION) != 0u : (flux_uia_patterns(info, node) & FLUX_UIA_P_SELECTION) != 0u;
		if (is_sel) return node;
	}
	for (XentNodeId p = flux_uia_parent(info, node); p != XENT_NODE_INVALID; p = flux_uia_parent(info, p)) {
		WatchEntry const *cached = watch_find(&w->table, p, -1);
		if (cached && (cached->has & W_SELECTION)) return p;
		if (!cached && (flux_uia_patterns(info, p) & FLUX_UIA_P_SELECTION)) return p;
		if (p == info->root) break;
	}
	return XENT_NODE_INVALID;
}

static void
watch_refresh_subitems(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, XentNodeId child_container) {
	int  first = 0, last = -1;
	bool has_subs = flux_uia_sub_range(info, node, &first, &last);
	watch_remove_stale_subs(&w->table, node, has_subs, first, last);
	if (!has_subs) return;
	for (int i = first; i <= last; i++) {
		WatchEntry now;
		watch_fill_entry(w, info, node, i, child_container, &now);
		WatchEntry const *old = watch_find(&w->table, node, i);
		watch_compare(w, info, old, &now);
		watch_track_sel(w, old, &now);
		if (watch_grow(&w->table)) watch_insert_raw(&w->table, &now);
	}
}

static void
watch_prime_new_children(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, XentNodeId child_container) {
	for (XentNodeId c = flux_uia_first(info, node); c != XENT_NODE_INVALID; c = flux_uia_next(info, c))
		if (!watch_find(&w->table, c, -1)) watch_walk_tree(w, info, c, -1, child_container, false);
}

static void watch_refresh_target(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId target, uint8_t flags) {
	WatchEntry *existing = watch_find(&w->table, target, -1);
	if (existing && existing->stamp == w->pump_gen && !(flags & FLUX_UIA_DIRTY_TREE)) return;

	XentNodeId container = watch_find_container(w, info, target, -1);
	WatchEntry now;
	watch_fill_entry(w, info, target, -1, container, &now);
	WatchEntry const *old = watch_find(&w->table, target, -1);
	if (!old || old->stamp != w->pump_gen) {
		watch_compare(w, info, old, &now);
		watch_track_sel(w, old, &now);
		if (watch_grow(&w->table)) watch_insert_raw(&w->table, &now);
	}
	XentNodeId child_container = (now.has & W_SELECTION) ? target : container;
	watch_refresh_subitems(w, info, target, child_container);
	if (flags & FLUX_UIA_DIRTY_TREE) watch_prime_new_children(w, info, target, child_container);
}

static XentNodeId watch_visible_in_tree(FluxUiaContext const *info, XentNodeId node) {
	for (XentNodeId cur = flux_uia_visible(info, node); cur != XENT_NODE_INVALID; cur = flux_uia_parent(info, cur)) {
		if (flux_uia_exposed(info, cur) && flux_uia_in_tree(info, cur)) return cur;
		if (cur == info->root) break;
	}
	return XENT_NODE_INVALID;
}

static void watch_process_dirty(FluxUiaWatch *w, FluxUiaContext const *info, XentNodeId node, uint8_t flags) {
	if (!flux_uia_in_tree(info, node)) {
		watch_remove_node_all(&w->table, node);
		if (xent_node_valid(info->ctx, node)) {
			XentNodeId anc = watch_visible_in_tree(info, node);
			if (anc != XENT_NODE_INVALID) watch_refresh_target(w, info, anc, flags | FLUX_UIA_DIRTY_TREE);
		}
		return;
	}
	XentNodeId target = flux_uia_visible(info, node);
	if (target == XENT_NODE_INVALID || !flux_uia_in_tree(info, target)) return;
	watch_refresh_target(w, info, target, flags);
	if (node != target || flux_node_type(info->store, node) == FLUX_CONTROL_TEXT) {
		XentNodeId parent = flux_uia_parent(info, target);
		if (parent != XENT_NODE_INVALID && flux_uia_in_tree(info, parent)) watch_refresh_target(w, info, parent, flags);
	}
}

static void watch_refresh_inplace(FluxUiaWatch *w, FluxUiaContext const *info, WatchEntry *slot) {
	WatchEntry now;
	watch_fill_entry(w, info, slot->node, slot->sub, slot->container, &now);
	watch_compare(w, info, slot, &now);
	now.slot_state = WATCH_OCCUPIED;
	*slot          = now;
}

static void watch_flush_sel_containers(FluxUiaWatch *w, FluxUiaContext const *info) {
	for (uint32_t c = 0u; c < w->nsel_containers; c++) {
		XentNodeId cont = w->sel_containers [c];
		for (uint32_t i = 0u; i < w->table.cap; i++) {
			WatchEntry *e = &w->table.e [i];
			if (e->slot_state != WATCH_OCCUPIED || e->container != cont || e->stamp == w->pump_gen) continue;
			if (!flux_uia_in_tree(info, e->node)) continue;
			watch_refresh_inplace(w, info, e);
		}
	}
}

static void watch_prune_table(FluxUiaWatch *w, FluxUiaContext const *info, bool check_tree) {
	for (uint32_t i = 0u; i < w->table.cap; i++) {
		WatchEntry const *e = &w->table.e [i];
		if (e->slot_state != WATCH_OCCUPIED || e->stamp == w->pump_gen) continue;
		if (!check_tree || !flux_uia_in_tree(info, e->node)) watch_remove_slot(&w->table, i);
	}
}

static void watch_raise_item(FluxUiaContext const *info, WatchChange const *c, int event_id) {
	FluxUia *p = c->sub < 0 ? flux_uia_alloc(info, c->node) : flux_uia_alloc_sub(info, c->node, c->sub);
	if (!p) return;
	UiaRaiseAutomationEvent(watch_simple(p), event_id);
	flux_uia_release(p);
}

static uint32_t watch_selected_in(WatchTable const *t, XentNodeId container) {
	uint32_t n = 0u;
	for (uint32_t i = 0u; i < t->cap; i++)
		n += t->e [i].slot_state == WATCH_OCCUPIED && t->e [i].container == container && t->e [i].selected;
	return n;
}

static WatchChange const *watch_first_of(FluxUiaWatch const *w, XentNodeId container, bool added) {
	for (uint32_t i = 0u; i < w->nchanges; i++)
		if (w->changes [i].container == container && w->changes [i].added == added) return &w->changes [i];
	return NULL;
}

static void watch_count(FluxUiaWatch const *w, XentNodeId container, uint32_t *added, uint32_t *removed) {
	*added = *removed = 0u;
	for (uint32_t i = 0u; i < w->nchanges; i++)
		if (w->changes [i].container == container) *(w->changes [i].added ? added : removed) += 1u;
}

static void watch_each_item(FluxUiaWatch const *w, FluxUiaContext const *info, XentNodeId container) {
	for (uint32_t i = 0u; i < w->nchanges; i++)
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
	if (added == 1u && watch_selected_in(&w->table, container) == 1u) {
		watch_raise_item(info, watch_first_of(w, container, true), WATCH_EVT_SELECTED);
		if (removed == 1u) watch_raise_item(info, watch_first_of(w, container, false), WATCH_EVT_REMOVED);
		return;
	}
	if (added + removed > WATCH_BULK_LIMIT) watch_invalidate(info, container);
	else watch_each_item(w, info, container);
}

static bool watch_seen_before(FluxUiaWatch const *w, uint32_t i) {
	for (uint32_t k = 0u; k < i; k++)
		if (w->changes [k].container == w->changes [i].container) return true;
	return false;
}

static void watch_selection(FluxUiaWatch *w, FluxUiaContext const *info) {
	for (uint32_t i = 0u; i < w->nchanges; i++)
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
	for (uint32_t i = 0u; listening && i < flux_store_nnotes(info->store); i++)
		watch_raise_note(info, flux_store_note(info->store, i));
	flux_store_clearnotes(info->store);
}

static void watch_drain_semantics(FluxUiaContext const *info, bool forward) {
	XentNodeId sem_buf [256];
	for (;;) {
		uint32_t n = xent_sem_drain_dirty(info->ctx, sem_buf, 256u);
		if (forward)
			for (uint32_t i = 0u; i < n; i++)
				flux_uia_mark_dirty(info->store, sem_buf [i], FLUX_UIA_DIRTY_PROPS | FLUX_UIA_DIRTY_PATTERN);
		if (n < 256u) break;
	}
}

static void watch_reset(FluxUiaWatch *w) {
	watch_clear(&w->table);
	w->nchanges        = 0u;
	w->nsel_containers = 0u;
	w->primed          = false;
}

FluxUiaWatch *flux_uia_watch_create(void) { return ( FluxUiaWatch * ) calloc(1, sizeof(FluxUiaWatch)); }

void          flux_uia_watch_destroy(FluxUiaWatch *w) {
	if (!w) return;
	free(w->table.e);
	free(w->changes);
	free(w->dirty_buf);
	free(w->sel_containers);
	free(w);
}

void flux_uia_watch_flush(FluxUiaContext const *info) {
	if (!info || !info->ctx || !info->store) return;
	watch_notes(info, UiaClientsAreListening());
}

static void watch_pump_all(FluxUiaWatch *w, FluxUiaContext const *info) {
	watch_walk_tree(w, info, info->root, -1, XENT_NODE_INVALID, true);
	watch_prune_table(w, info, false);
	watch_selection(w, info);
}

static void watch_pump_dirty(FluxUiaWatch *w, FluxUiaContext const *info, uint32_t ndirty) {
	bool tree_dirty = false;
	for (uint32_t i = 0u; i < ndirty; i++) {
		tree_dirty |= (w->dirty_buf [i].flags & FLUX_UIA_DIRTY_TREE) != 0u;
		watch_process_dirty(w, info, w->dirty_buf [i].node, w->dirty_buf [i].flags);
	}
	watch_flush_sel_containers(w, info);
	if (tree_dirty) watch_prune_table(w, info, true);
	watch_selection(w, info);
}

static void watch_prime_tree(FluxUiaWatch *w, FluxUiaContext const *info) {
	flux_uia_clear_dirty(info->store);
	watch_drain_semantics(info, false);
	w->pump_gen++;
	watch_walk_tree(w, info, info->root, -1, XENT_NODE_INVALID, false);
	w->primed = true;
}

static bool watch_ensure_ready(FluxUiaWatch *w, FluxUiaContext const *info) {
	bool listening = UiaClientsAreListening();
	watch_notes(info, listening);
	if (!listening) {
		if (w->primed) watch_reset(w);
		flux_uia_clear_dirty(info->store);
		watch_drain_semantics(info, false);
		return false;
	}
	if (!w->primed) {
		watch_prime_tree(w, info);
		return false;
	}
	return true;
}

void flux_uia_watch_pump(FluxUiaWatch *w, FluxUiaContext const *info) {
	if (!w || !info || !info->ctx || info->root == XENT_NODE_INVALID) return;
	if (!watch_ensure_ready(w, info)) return;
	watch_drain_semantics(info, true);
	bool     all    = false;
	uint32_t ndirty = flux_uia_drain_dirty(info->store, &w->dirty_buf, &w->dirty_cap, &all);
	if (!all && ndirty == 0u) return;
	w->pump_gen++;
	w->nsel_containers = 0u;
	w->nchanges        = 0u;
	if (all) watch_pump_all(w, info);
	else watch_pump_dirty(w, info, ndirty);
}
