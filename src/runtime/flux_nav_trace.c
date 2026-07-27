#include <fluxent/flux_nav_trace.h>

#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_scroll_data.h"
#include "store/flux_node_data.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define TRACE_ORIGIN_CAP 256
#define TRACE_SCROLL_CAP 128

typedef struct TraceOrigin {
	uint64_t id;
	char     text [160];
} TraceOrigin;

typedef struct NavState {
	XentNodeId node;
	int        count;
	int        selected;
	int        mode;
	int        flyout_parent;
	int        indicator_index;
	int        indicator_visible;
	uint64_t   selection_revision;
	float      width;
	float      height;
	float      pane_width;
	float      minimal_fraction;
	float      indicator_top;
	float      indicator_bottom;
	float      indicator_opacity;
	bool       minimal_open;
	bool       adaptive;
	bool       fill_window;
	bool       window_title_bar;
	bool       indicator_in_scroll;
	bool       indicator_primed;
	bool       anim_active;
	bool       labels_visible;
	bool       valid;
} NavState;

typedef struct ScrollState {
	XentNodeId node;
	float      x;
	float      y;
	bool       seen;
} ScrollState;

static SRWLOCK                     g_trace_lock = SRWLOCK_INIT;
static TraceOrigin                 g_origins [TRACE_ORIGIN_CAP];
static NavState                    g_nav_state;
static ScrollState                 g_scroll_state [TRACE_SCROLL_CAP];
static volatile LONG64             g_trace_next;
static __declspec(thread) uint64_t g_trace_current;
static char                        g_trace_path [MAX_PATH];

static void                        trace_set_path(void) {
	DWORD len = GetModuleFileNameA(NULL, g_trace_path, sizeof(g_trace_path));
	if (!len || len >= sizeof(g_trace_path)) {
		snprintf(
		  g_trace_path, sizeof(g_trace_path), "fluxent.nav_trace.%lu.%llu.log", ( unsigned long ) GetCurrentProcessId(),
		  ( unsigned long long ) GetTickCount64()
		);
		return;
	}
	char *slash = strrchr(g_trace_path, '\\');
	char *other = strrchr(g_trace_path, '/');
	if (!slash || (other && other > slash)) slash = other;
	char *dot = strrchr(slash ? slash + 1 : g_trace_path, '.');
	if (dot) *dot = '\0';
	char suffix [80];
	snprintf(
	  suffix, sizeof(suffix), ".nav_trace.%lu.%llu.log", ( unsigned long ) GetCurrentProcessId(),
	  ( unsigned long long ) GetTickCount64()
	);
	strncat(g_trace_path, suffix, sizeof(g_trace_path) - strlen(g_trace_path) - 1u);
}

static char const *trace_origin(uint64_t id, char *fallback, size_t cap) {
	TraceOrigin const *slot = &g_origins [id % TRACE_ORIGIN_CAP];
	if (slot->id == id) return slot->text;
	snprintf(fallback, cap, "unknown:%llu", ( unsigned long long ) id);
	return fallback;
}

static void trace_write(uint64_t id, char const *stage, char const *message) {
	char  fallback [48];
	char  line [768];
	DWORD now = GetTickCount();
	AcquireSRWLockShared(&g_trace_lock);
	char const *origin = trace_origin(id, fallback, sizeof(fallback));
	snprintf(
	  line, sizeof(line), "[%10lu] trace=%llu origin=\"%s\" %-20s %s\n", ( unsigned long ) now,
	  ( unsigned long long ) id, origin, stage, message
	);
	ReleaseSRWLockShared(&g_trace_lock);

	FILE *file = fopen(g_trace_path, "ab");
	if (file) {
		fwrite(line, 1, strlen(line), file);
		fclose(file);
	}
	OutputDebugStringA(line);
}

void flux_nav_trace_reset(void) {
	trace_set_path();
	AcquireSRWLockExclusive(&g_trace_lock);
	memset(g_origins, 0, sizeof(g_origins));
	memset(&g_nav_state, 0, sizeof(g_nav_state));
	memset(g_scroll_state, 0, sizeof(g_scroll_state));
	g_trace_next    = 0;
	g_trace_current = 0;
	FILE *file      = fopen(g_trace_path, "wb");
	if (file) {
		fprintf(file, "FluXent NavigationView causal trace\npath=%s\n", g_trace_path);
		fclose(file);
	}
	ReleaseSRWLockExclusive(&g_trace_lock);
}

uint64_t flux_nav_trace_begin(char const *format, ...) {
	uint64_t     id   = ( uint64_t ) InterlockedIncrement64(&g_trace_next);
	TraceOrigin *slot = &g_origins [id % TRACE_ORIGIN_CAP];
	AcquireSRWLockExclusive(&g_trace_lock);
	slot->id = id;
	va_list args;
	va_start(args, format);
	vsnprintf(slot->text, sizeof(slot->text), format, args);
	va_end(args);
	ReleaseSRWLockExclusive(&g_trace_lock);
	g_trace_current = id;
	trace_write(id, "origin", "begin");
	return id;
}

void flux_nav_trace_adopt(uint64_t trace_id) {
	if (trace_id) g_trace_current = trace_id;
}

void     flux_nav_trace_clear(void) { g_trace_current = 0; }

uint64_t flux_nav_trace_current(void) { return g_trace_current; }

void     flux_nav_trace_log(char const *stage, char const *format, ...) {
	char    message [448];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	uint64_t id = g_trace_current;
	if (!id) id = flux_nav_trace_begin("internal-without-origin");
	trace_write(id, stage, message);
}

static XentNodeId find_nav(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return node;
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd && nd->component_type == FLUX_CONTROL_NAV_VIEW) return node;
	for (XentNodeId child = xent_node_first(ctx, node); child != XENT_NODE_INVALID; child = xent_node_next(ctx, child))
	{
		XentNodeId found = find_nav(ctx, store, child);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static NavState read_nav_state(FluxStore *store, XentNodeId node) {
	FluxNodeData    *nd = flux_store_find(store, node);
	FluxNavViewData *d  = nd ? ( FluxNavViewData * ) nd->component_data : NULL;
	NavState         state;
	memset(&state, 0, sizeof(state));
	if (!d) return state;
	state.node                = node;
	state.count               = d->count;
	state.selected            = d->selected;
	state.mode                = d->mode;
	state.flyout_parent       = d->flyout_parent;
	state.indicator_index     = d->ind_index;
	state.indicator_visible   = d->ind_visible;
	state.selection_revision  = d->selection_revision;
	state.width               = d->width;
	state.height              = d->height;
	state.pane_width          = d->pane_w.current;
	state.minimal_fraction    = d->minimal_t.current;
	state.indicator_top       = d->ind_top.current;
	state.indicator_bottom    = d->ind_bot.current;
	state.indicator_opacity   = d->ind_op.current;
	state.minimal_open        = d->minimal_open;
	state.adaptive            = d->adaptive;
	state.fill_window         = d->fill_window;
	state.window_title_bar    = d->window_title_bar;
	state.indicator_in_scroll = d->ind_in_scroll;
	state.indicator_primed    = d->ind_primed;
	state.anim_active         = d->anim_active;
	state.labels_visible      = d->labels_visible;
	state.valid               = true;
	return state;
}

static bool nav_state_equal(NavState const *a, NavState const *b) { return memcmp(a, b, sizeof(*a)) == 0; }

static void trace_nav_state(FluxStore *store, XentNodeId node, char const *stage) {
	NavState next = read_nav_state(store, node);
	if (nav_state_equal(&next, &g_nav_state)) return;
	flux_nav_trace_log(
	  stage,
	  "nav node=%llu count=%d selected=%d mode=%d minimal_open=%d adaptive=%d "
	  "flyout=%d revision=%llu size=%.1fx%.1f pane=%.2f minimal=%.3f "
	  "indicator=%d/%d %.2f..%.2f opacity=%.3f in_scroll=%d primed=%d "
	  "anim=%d labels=%d fill_window=%d title_bar=%d",
	  ( unsigned long long ) next.node, next.count, next.selected, next.mode, next.minimal_open, next.adaptive,
	  next.flyout_parent, ( unsigned long long ) next.selection_revision, next.width, next.height, next.pane_width,
	  next.minimal_fraction, next.indicator_index, next.indicator_visible, next.indicator_top, next.indicator_bottom,
	  next.indicator_opacity, next.indicator_in_scroll, next.indicator_primed, next.anim_active, next.labels_visible,
	  next.fill_window, next.window_title_bar
	);
	g_nav_state = next;
}

static ScrollState *scroll_slot(XentNodeId node) {
	for (int i = 0; i < TRACE_SCROLL_CAP; i++)
		if (g_scroll_state [i].seen && g_scroll_state [i].node == node) return &g_scroll_state [i];
	for (int i = 0; i < TRACE_SCROLL_CAP; i++)
		if (!g_scroll_state [i].seen) return &g_scroll_state [i];
	return &g_scroll_state [xent_node_index(node) % TRACE_SCROLL_CAP];
}

static void trace_scroll(XentCtx *ctx, FluxStore *store, XentNodeId node, char const *stage) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd && nd->component_type == FLUX_CONTROL_SCROLL && nd->component_data) {
		FluxScrollData *d    = ( FluxScrollData * ) nd->component_data;
		ScrollState    *slot = scroll_slot(node);
		if (!slot->seen || slot->x != d->scroll_x || slot->y != d->scroll_y) {
			flux_nav_trace_log(
			  stage, "scroll node=%llu x=%.2f y=%.2f", ( unsigned long long ) node, d->scroll_x, d->scroll_y
			);
			*slot = (ScrollState) {node, d->scroll_x, d->scroll_y, true};
		}
	}
	for (XentNodeId child = xent_node_first(ctx, node); child != XENT_NODE_INVALID; child = xent_node_next(ctx, child))
		trace_scroll(ctx, store, child, stage);
}

void flux_nav_trace_snapshot(XentCtx *ctx, FluxStore *store, XentNodeId root, char const *stage) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return;
	trace_nav_state(store, find_nav(ctx, store, root), stage);
	trace_scroll(ctx, store, root, stage);
}
