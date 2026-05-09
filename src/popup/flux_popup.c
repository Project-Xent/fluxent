#include "fluxent/flux_popup.h"
#include "render/flux_scroll_geom.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <windows.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Windows 11 DWM backdrop attributes (some SDKs are missing these). */
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
  #define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
  #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
  #define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMSBT_NONE
  #define DWMSBT_NONE 1
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
  #define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DEFAULT
  #define DWMWCP_DEFAULT    0
  #define DWMWCP_DONOTROUND 1
  #define DWMWCP_ROUND      2
  #define DWMWCP_ROUNDSMALL 3
#endif

typedef struct FluxPopupAnim {
	FluxPopupAnimStyle style;
	bool               active;
	bool               is_layered;
	int                final_x, final_y, final_pw, final_ph;
	FluxPlacement      final_placement;
} FluxPopupAnim;

struct FluxPopup {
	FluxWindow              *owner;
	HWND                     owner_hwnd;
	HWND                     popup_hwnd;
	FluxGraphics            *graphics;

	XentContext             *ctx;
	FluxNodeStore           *store;
	XentNodeId               content_root;

	FluxRect                 anchor;
	FluxPlacement            placement;
	float                    content_w;
	float                    content_h;

	bool                     is_visible;
	bool                     dismiss_on_outside;

	FluxPopupDismissCallback dismiss_cb;
	void                    *dismiss_ctx;

	FluxPopupPaintCallback   paint_cb;
	void                    *paint_ctx;

	FluxPopupMouseCallback   mouse_cb;
	void                    *mouse_ctx;
	bool                     mouse_tracking;

	bool                     backdrop_enabled;
	float                    max_content_h; /* 0 = no clamp */

	FluxPopupAnim            anim;
};

static FluxPopup **g_popup_registry       = NULL;
static int         g_popup_registry_count = 0;
static int         g_popup_registry_cap   = 0;

static int         popup_registry_find(FluxPopup *p) {
	for (int i = 0; i < g_popup_registry_count; i++)
		if (g_popup_registry [i] == p) return i;
	return -1;
}

static void popup_registry_add(FluxPopup *p) {
	if (!p) return;
	if (popup_registry_find(p) >= 0) return;
	if (g_popup_registry_count >= g_popup_registry_cap) {
		int         new_cap = g_popup_registry_cap ? g_popup_registry_cap * 2 : 64;
		FluxPopup **items   = ( FluxPopup ** ) realloc(g_popup_registry, ( size_t ) new_cap * sizeof(*items));
		if (!items) return;
		g_popup_registry     = items;
		g_popup_registry_cap = new_cap;
	}
	g_popup_registry [g_popup_registry_count++] = p;
}

static void popup_registry_remove(FluxPopup *p) {
	int idx = popup_registry_find(p);
	if (idx < 0) return;
	g_popup_registry [idx] = g_popup_registry [--g_popup_registry_count];
	if (g_popup_registry_count == 0) {
		free(g_popup_registry);
		g_popup_registry     = NULL;
		g_popup_registry_cap = 0;
	}
}

/* WinUI 3 MenuPopupThemeTransition: ScaleY 0.5→1.0 from anchor edge, 250 ms, Cubic-Bezier(0,0,0,1).
 * Source: microsoft-ui-xaml MenuPopupThemeTransition_Partial.h, closedRatio=0.5. */
/* PopupThemeTransition (Flyout): translate 50 DIP + fade, 167 ms, ease-out. */
#define POPUP_FLYOUT_OFFSET_DIP 50.0f

static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void             popup_position(FluxPopup *popup);
static wchar_t const   *POPUP_CLASS_NAME = L"FluxPopupClass";
static ATOM             s_popup_class    = 0;

static void             ensure_popup_class(void) {
	if (s_popup_class) return;
	WNDCLASSEXW wc   = {0};
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = popup_wnd_proc;
	wc.hInstance     = GetModuleHandleW(NULL);
	wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszClassName = POPUP_CLASS_NAME;
	s_popup_class    = RegisterClassExW(&wc);
}

FluxPopup *flux_popup_create(FluxWindow *owner) {
	if (!owner) return NULL;

	ensure_popup_class();

	FluxPopup *popup = ( FluxPopup * ) calloc(1, sizeof(*popup));
	if (!popup) return NULL;

	popup->owner              = owner;
	popup->owner_hwnd         = flux_window_hwnd(owner);
	popup->dismiss_on_outside = true;
	popup->content_w          = 200.0f;
	popup->content_h          = 100.0f;
	popup->placement          = FLUX_PLACEMENT_BOTTOM;
	popup->content_root       = XENT_NODE_INVALID;

	/* WS_EX_NOREDIRECTIONBITMAP must be set at creation — DWM ignores it if added later via SetWindowLongPtrW. */
	popup->popup_hwnd         = CreateWindowExW(
	  WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP, POPUP_CLASS_NAME, L"",
	  WS_POPUP | WS_CLIPSIBLINGS, 0, 0, 1, 1, popup->owner_hwnd, NULL, GetModuleHandleW(NULL), popup
	);

	if (!popup->popup_hwnd) {
		free(popup);
		return NULL;
	}

	SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, ( LONG_PTR ) popup);
	popup_registry_add(popup);

	popup->graphics = flux_graphics_create();
	if (!popup->graphics) {
		DestroyWindow(popup->popup_hwnd);
		free(popup);
		return NULL;
	}

	HRESULT hr = flux_graphics_attach(popup->graphics, popup->popup_hwnd);
	if (FAILED(hr)) {
		flux_graphics_destroy(popup->graphics);
		DestroyWindow(popup->popup_hwnd);
		free(popup);
		return NULL;
	}

	return popup;
}

void flux_popup_destroy(FluxPopup *popup) {
	if (!popup) return;
	popup_registry_remove(popup);
	if (popup->popup_hwnd) {
		SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, 0);
		DestroyWindow(popup->popup_hwnd);
	}
	if (popup->graphics) flux_graphics_destroy(popup->graphics);
	free(popup);
}

void flux_popup_set_content(FluxPopup *popup, XentContext *ctx, FluxNodeStore *store, XentNodeId content_root) {
	if (!popup) return;
	popup->ctx          = ctx;
	popup->store        = store;
	popup->content_root = content_root;
}

void flux_popup_set_size(FluxPopup *popup, float width, float height) {
	if (!popup) return;
	popup->content_w = width;
	popup->content_h = height;
	if (popup->is_visible) popup_position(popup);
}

void flux_popup_set_dismiss_on_outside(FluxPopup *popup, bool dismiss) {
	if (!popup) return;
	popup->dismiss_on_outside = dismiss;
}

void flux_popup_set_dismiss_callback(FluxPopup *popup, FluxPopupDismissCallback cb, void *ctx) {
	if (!popup) return;
	popup->dismiss_cb  = cb;
	popup->dismiss_ctx = ctx;
}

/* WS_EX_LAYERED for per-window alpha animation; DComp/DWM backdrop composites correctly under LWA_ALPHA. */
static void popup_ensure_layered(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd || popup->anim.is_layered) return;
	LONG_PTR ex = GetWindowLongPtrW(popup->popup_hwnd, GWL_EXSTYLE);
	SetWindowLongPtrW(popup->popup_hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
	SetLayeredWindowAttributes(popup->popup_hwnd, 0, 255, LWA_ALPHA);
	popup->anim.is_layered = true;
}

void flux_popup_set_anim_style(FluxPopup *popup, FluxPopupAnimStyle style) {
	if (!popup) return;
	popup->anim.style = style;
	/* FLYOUT and TOOLTIP animate window opacity — need WS_EX_LAYERED. */
	if (style == FLUX_POPUP_ANIM_FLYOUT || style == FLUX_POPUP_ANIM_TOOLTIP) popup_ensure_layered(popup);
}

static float popup_scale_for_owner(FluxPopup const *popup) {
	UINT dpi = GetDpiForWindow(popup->owner_hwnd);
	return dpi / 96.0f;
}

static RECT popup_owner_work_rect(FluxPopup const *popup) {
	HMONITOR    mon = MonitorFromWindow(popup->owner_hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi  = {.cbSize = sizeof(mi)};
	GetMonitorInfoW(mon, &mi);
	return mi.rcWork;
}

static void popup_clamp_content_height(FluxPopup const *popup, RECT const *work, float scale, float *content_h) {
	float const screen_margin = 8.0f;
	float       work_h_dip    = (( float ) (work->bottom - work->top)) / scale - 2.0f * screen_margin;

	if (work_h_dip < 64.0f) work_h_dip = 64.0f;
	if (popup->max_content_h > 0.0f && popup->max_content_h < work_h_dip) work_h_dip = popup->max_content_h;
	if (*content_h > work_h_dip) *content_h = work_h_dip;
}

static bool          popup_anchor_is_control(FluxRect const *anchor) { return anchor->w > 0.5f || anchor->h > 0.5f; }

static FluxPlacement popup_effective_placement(FluxPlacement placement) {
	return placement == FLUX_PLACEMENT_AUTO ? FLUX_PLACEMENT_BOTTOM : placement;
}

static FluxPlacement popup_flip_placement(FluxPlacement placement) {
	switch (placement) {
	case FLUX_PLACEMENT_BOTTOM : return FLUX_PLACEMENT_TOP;
	case FLUX_PLACEMENT_TOP    : return FLUX_PLACEMENT_BOTTOM;
	case FLUX_PLACEMENT_RIGHT  : return FLUX_PLACEMENT_LEFT;
	case FLUX_PLACEMENT_LEFT   : return FLUX_PLACEMENT_RIGHT;
	case FLUX_PLACEMENT_AUTO   :
	default                    : return FLUX_PLACEMENT_BOTTOM;
	}
}

typedef struct PopupPositionParams {
	FluxRect const *anchor;
	FluxPlacement   placement;
	float           aw;
	float           ah;
	float           scale;
	int             pw;
	int             ph;
} PopupPositionParams;

typedef struct PopupPoint {
	int x;
	int y;
} PopupPoint;

typedef struct PopupWindowMessage {
	HWND       hwnd;
	FluxPopup *popup;
	UINT       msg;
	WPARAM     wp;
	LPARAM     lp;
} PopupWindowMessage;

typedef struct PopupMouseCoord {
	float x;
	float y;
} PopupMouseCoord;

static PopupPoint popup_position_bottom(PopupPositionParams const *p, bool anchored) {
	return (PopupPoint) {
	  anchored ? ( int ) (p->anchor->x + (p->anchor->w - p->aw * p->scale) * 0.5f) : ( int ) p->anchor->x,
	  anchored ? ( int ) (p->anchor->y + p->anchor->h + 4 * p->scale) : ( int ) (p->anchor->y + p->anchor->h),
	};
}

static PopupPoint popup_position_top(PopupPositionParams const *p, bool anchored) {
	return (PopupPoint) {
	  anchored ? ( int ) (p->anchor->x + (p->anchor->w - p->aw * p->scale) * 0.5f) : ( int ) p->anchor->x,
	  anchored ? ( int ) (p->anchor->y - p->ph - 4 * p->scale) : ( int ) (p->anchor->y - p->ph),
	};
}

static PopupPoint popup_position_right(PopupPositionParams const *p, bool anchored) {
	return (PopupPoint) {
	  ( int ) (p->anchor->x + p->anchor->w + (anchored ? 4 * p->scale : 0)),
	  ( int ) (p->anchor->y + (p->anchor->h - p->ah * p->scale) * 0.5f),
	};
}

static PopupPoint popup_position_left(PopupPositionParams const *p, bool anchored) {
	return (PopupPoint) {
	  ( int ) (p->anchor->x - p->pw - (anchored ? 4 * p->scale : 0)),
	  ( int ) (p->anchor->y + (p->anchor->h - p->ah * p->scale) * 0.5f),
	};
}

static void popup_compute_position(PopupPositionParams const *p, int *x, int *y) {
	bool       anchored = popup_anchor_is_control(p->anchor);
	PopupPoint point    = {( int ) p->anchor->x, ( int ) (p->anchor->y + p->anchor->h)};

	switch (p->placement) {
	case FLUX_PLACEMENT_BOTTOM : point = popup_position_bottom(p, anchored); break;
	case FLUX_PLACEMENT_TOP    : point = popup_position_top(p, anchored); break;
	case FLUX_PLACEMENT_RIGHT  : point = popup_position_right(p, anchored); break;
	case FLUX_PLACEMENT_LEFT   : point = popup_position_left(p, anchored); break;
	case FLUX_PLACEMENT_AUTO   :
	default                    : break;
	}
	*x = point.x;
	*y = point.y;
}

static bool popup_needs_flip(PopupPositionParams const *p, RECT const *work, int x, int y) {
	switch (p->placement) {
	case FLUX_PLACEMENT_BOTTOM : return y + p->ph > work->bottom;
	case FLUX_PLACEMENT_TOP    : return y < work->top;
	case FLUX_PLACEMENT_RIGHT  : return x + p->pw > work->right;
	case FLUX_PLACEMENT_LEFT   : return x < work->left;
	case FLUX_PLACEMENT_AUTO   :
	default                    : return false;
	}
}

static void popup_clamp_window_rect(RECT const *work, int pw, int ph, int *x, int *y) {
	if (*x + pw > work->right) *x = work->right - pw;
	if (*x < work->left) *x = work->left;
	if (*y + ph > work->bottom) *y = work->bottom - ph;
	if (*y < work->top) *y = work->top;
}

static void popup_position(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd) return;

	float         aw        = popup->content_w;
	float         ah        = popup->content_h;
	float         scale     = popup_scale_for_owner(popup);
	RECT          work      = popup_owner_work_rect(popup);
	int           x         = 0;
	int           y         = 0;
	FluxPlacement placement = popup_effective_placement(popup->placement);

	popup_clamp_content_height(popup, &work, scale, &ah);

	int                 pw = ( int ) (aw * scale + 0.5f);
	int                 ph = ( int ) (ah * scale + 0.5f);

	PopupPositionParams params
	  = {.anchor = &popup->anchor, .placement = placement, .aw = aw, .ah = ah, .scale = scale, .pw = pw, .ph = ph};

	popup_compute_position(&params, &x, &y);
	if (popup_needs_flip(&params, &work, x, y)) {
		params.placement = popup_flip_placement(params.placement);
		popup_compute_position(&params, &x, &y);
	}
	popup_clamp_window_rect(&work, pw, ph, &x, &y);

	popup->anim.final_x         = x;
	popup->anim.final_y         = y;
	popup->anim.final_pw        = pw;
	popup->anim.final_ph        = ph;
	popup->anim.final_placement = params.placement;

	SetWindowPos(popup->popup_hwnd, HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);
	flux_graphics_resize(popup->graphics, pw, ph);
}

/* Resize HWND each frame so DWM backdrop tracks ScaleY — skips flux_graphics_resize() to avoid ResizeBuffers judder. */
static void popup_apply_animated_rect(FluxPopup *popup, float scaleY) {
	if (!popup || !popup->popup_hwnd || popup->anim.final_ph <= 0) return;

	int cur_ph = ( int ) (( float ) popup->anim.final_ph * scaleY + 0.5f);
	if (cur_ph < 1) cur_ph = 1;

	int cur_y = popup->anim.final_y;
	if (popup->anim.final_placement == FLUX_PLACEMENT_TOP)
		cur_y = popup->anim.final_y + (popup->anim.final_ph - cur_ph);

	SetWindowPos(
	  popup->popup_hwnd, HWND_TOPMOST, popup->anim.final_x, cur_y, popup->anim.final_pw, cur_ph, SWP_NOACTIVATE
	);
}

/* Content slides from offset into final position along placement axis. */
static void popup_apply_flyout_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd) return;

	UINT  dpi       = GetDpiForWindow(popup->owner_hwnd);
	float scale     = dpi > 0 ? dpi / 96.0f : 1.0f;
	int   offset_px = ( int ) (POPUP_FLYOUT_OFFSET_DIP * scale + 0.5f);
	int   from      = ( int ) ((1.0f - t) * ( float ) offset_px + 0.5f);
	int   dx = 0, dy = 0;

	switch (popup->anim.final_placement) {
	case FLUX_PLACEMENT_BOTTOM : dy = -from; break;
	case FLUX_PLACEMENT_TOP    : dy = +from; break;
	case FLUX_PLACEMENT_RIGHT  : dx = -from; break;
	case FLUX_PLACEMENT_LEFT   : dx = +from; break;
	default                    : break;
	}

	SetWindowPos(
	  popup->popup_hwnd, HWND_TOPMOST, popup->anim.final_x + dx, popup->anim.final_y + dy, popup->anim.final_pw,
	  popup->anim.final_ph, SWP_NOACTIVATE | SWP_NOREDRAW
	);

	BYTE alpha = ( BYTE ) (t * 255.0f + 0.5f);
	if (popup->anim.is_layered) SetLayeredWindowAttributes(popup->popup_hwnd, 0, alpha, LWA_ALPHA);
}

static void popup_apply_tooltip_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd || !popup->anim.is_layered) return;
	BYTE alpha = ( BYTE ) (t * 255.0f + 0.5f);
	SetLayeredWindowAttributes(popup->popup_hwnd, 0, alpha, LWA_ALPHA);
}

void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement) {
	if (!popup) return;

	popup->anchor     = anchor;
	popup->placement  = placement;
	popup->is_visible = true;

	popup_position(popup);

	popup->anim.active = false;

	switch (popup->anim.style) {
	case FLUX_POPUP_ANIM_FLYOUT  : popup_apply_flyout_frame(popup, 1.0f); break;
	case FLUX_POPUP_ANIM_TOOLTIP : popup_apply_tooltip_frame(popup, 1.0f); break;
	case FLUX_POPUP_ANIM_NONE    : break;
	case FLUX_POPUP_ANIM_MENU    :
	default                      : popup_apply_animated_rect(popup, 1.0f); break;
	}

	ShowWindow(popup->popup_hwnd, SW_SHOWNOACTIVATE);
	InvalidateRect(popup->popup_hwnd, NULL, FALSE);
}

void flux_popup_dismiss(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;

	popup->is_visible  = false;

	popup->anim.active = false;

	ShowWindow(popup->popup_hwnd, SW_HIDE);
	if (popup->dismiss_cb) popup->dismiss_cb(popup->dismiss_ctx);
}

void flux_popup_update_position(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;
	popup_position(popup);
}

bool flux_popup_is_visible(FluxPopup const *popup) { return popup ? popup->is_visible : false; }

void flux_popup_set_paint_callback(FluxPopup *popup, FluxPopupPaintCallback cb, void *ctx) {
	if (!popup) return;
	popup->paint_cb  = cb;
	popup->paint_ctx = ctx;
}

void flux_popup_set_mouse_callback(FluxPopup *popup, FluxPopupMouseCallback cb, void *ctx) {
	if (!popup) return;
	popup->mouse_cb  = cb;
	popup->mouse_ctx = ctx;
}

FluxGraphics *flux_popup_get_graphics(FluxPopup *popup) { return popup ? popup->graphics : NULL; }

HWND          flux_popup_get_hwnd(FluxPopup *popup) { return popup ? popup->popup_hwnd : NULL; }

bool          flux_popup_enable_system_backdrop(FluxPopup *popup, bool is_dark) {
	if (!popup || !popup->popup_hwnd) return false;

	/* Dark-mode tint: set before backdrop type so first DWM frame uses correct tint. */
	BOOL dark = is_dark ? TRUE : FALSE;
	DwmSetWindowAttribute(popup->popup_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	/* DWMWCP_ROUND clips DWM acrylic to match the 8-DIP D2D card corners. */
	int corner = DWMWCP_ROUND;
	DwmSetWindowAttribute(popup->popup_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

	int     backdrop = DWMSBT_TRANSIENTWINDOW;
	HRESULT hr       = DwmSetWindowAttribute(popup->popup_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
	if (SUCCEEDED(hr)) {
		popup->backdrop_enabled = true;
		return true;
	}
	popup->backdrop_enabled = false;
	return false;
}

bool flux_popup_has_system_backdrop(FluxPopup const *popup) { return popup ? popup->backdrop_enabled : false; }

void flux_popup_set_max_content_height(FluxPopup *popup, float max_h) {
	if (!popup) return;
	popup->max_content_h = (max_h > 0.0f) ? max_h : 0.0f;
	if (popup->is_visible) popup_position(popup);
}

void flux_popup_dismiss_all_for_owner(HWND owner_hwnd) {
	if (!owner_hwnd) return;
	int         cap        = g_popup_registry_count;
	FluxPopup **to_dismiss = cap > 0 ? ( FluxPopup ** ) malloc(( size_t ) cap * sizeof(*to_dismiss)) : NULL;
	int         n          = 0;
	if (cap > 0 && !to_dismiss) return;

	for (int i = 0; i < g_popup_registry_count; i++) {
		FluxPopup *p = g_popup_registry [i];
		if (p && p->is_visible && p->owner_hwnd == owner_hwnd) to_dismiss [n++] = p;
	}
	for (int i = 0; i < n; i++) flux_popup_dismiss(to_dismiss [i]);
	free(to_dismiss);
}

static float popup_window_scale(HWND hwnd) {
	UINT dpi = GetDpiForWindow(hwnd);
	return dpi > 0 ? ( float ) dpi / 96.0f : 1.0f;
}

static PopupMouseCoord popup_mouse_point(HWND hwnd, LPARAM lp) {
	float scale = popup_window_scale(hwnd);
	return (PopupMouseCoord) {
	  ( float ) ( short ) LOWORD(lp) / scale,
	  ( float ) ( short ) HIWORD(lp) / scale,
	};
}

static PopupMouseCoord popup_wheel_point(HWND hwnd, WPARAM wp, LPARAM lp) {
	float scale = popup_window_scale(hwnd);
	POINT pt    = {( short ) LOWORD(lp), ( short ) HIWORD(lp)};
	ScreenToClient(hwnd, &pt);
	return (PopupMouseCoord) {
	  ( float ) pt.x / scale,
	  (( float ) GET_WHEEL_DELTA_WPARAM(wp) / ( float ) WHEEL_DELTA) * FLUX_SCROLLBAR_SMALL_CHANGE,
	};
}

static void popup_begin_mouse_tracking(HWND hwnd, FluxPopup *popup) {
	if (popup->mouse_tracking) return;

	TRACKMOUSEEVENT tme;
	tme.cbSize      = sizeof(tme);
	tme.dwFlags     = TME_LEAVE;
	tme.hwndTrack   = hwnd;
	tme.dwHoverTime = 0;
	TrackMouseEvent(&tme);
	popup->mouse_tracking = true;
}

static LRESULT popup_on_mouse_event(PopupWindowMessage const *m, FluxPopupMouseEvent ev) {
	FluxPopup *popup = m->popup;
	if (!popup || !popup->mouse_cb) return 0;

	if (ev == FLUX_POPUP_MOUSE_MOVE) popup_begin_mouse_tracking(m->hwnd, popup);

	PopupMouseCoord point
	  = ev == FLUX_POPUP_MOUSE_WHEEL ? popup_wheel_point(m->hwnd, m->wp, m->lp) : popup_mouse_point(m->hwnd, m->lp);
	popup->mouse_cb(popup->mouse_ctx, popup, ev, point.x, point.y);
	return 0;
}

static LRESULT popup_on_mouse_leave(FluxPopup *popup) {
	if (!popup) return 0;

	popup->mouse_tracking = false;
	if (popup->mouse_cb) popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_LEAVE, -1.0f, -1.0f);
	return 0;
}

static float popup_menu_top_translate_y(FluxPopup *popup, HWND hwnd) {
	if (!popup->anim.active || popup->anim.style != FLUX_POPUP_ANIM_MENU) return 0.0f;
	if (popup->anim.final_placement != FLUX_PLACEMENT_TOP) return 0.0f;

	float scale = popup_window_scale(hwnd);
	RECT  wr;
	GetClientRect(hwnd, &wr);

	float cur_h_dip = (( float ) (wr.bottom - wr.top)) / scale;
	float fin_h_dip = (( float ) popup->anim.final_ph) / scale;
	return cur_h_dip - fin_h_dip;
}

static void popup_set_rt_translate(ID2D1RenderTarget *rt, float translate_y) {
	if (!rt || fabsf(translate_y) <= 0.1f) return;

	D2D1_MATRIX_3X2_F xform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, translate_y};
	ID2D1RenderTarget_SetTransform(rt, &xform);
}

static void popup_reset_rt_transform(ID2D1RenderTarget *rt, float translate_y) {
	if (!rt || fabsf(translate_y) <= 0.1f) return;

	D2D1_MATRIX_3X2_F identity = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	ID2D1RenderTarget_SetTransform(rt, &identity);
}

static LRESULT popup_on_paint(HWND hwnd, FluxPopup *popup) {
	PAINTSTRUCT ps;
	BeginPaint(hwnd, &ps);
	if (!popup || !popup->graphics) {
		EndPaint(hwnd, &ps);
		return 0;
	}

	flux_graphics_begin_draw(popup->graphics);
	flux_graphics_clear(popup->graphics, flux_color_rgba(0, 0, 0, 0));

	ID2D1RenderTarget *rt          = ( ID2D1RenderTarget * ) flux_graphics_get_d2d_context(popup->graphics);
	float              translate_y = popup_menu_top_translate_y(popup, hwnd);

	popup_set_rt_translate(rt, translate_y);
	if (popup->paint_cb) popup->paint_cb(popup->paint_ctx, popup);
	popup_reset_rt_transform(rt, translate_y);

	flux_graphics_end_draw(popup->graphics);
	flux_graphics_present(popup->graphics, true);
	flux_graphics_commit(popup->graphics);
	EndPaint(hwnd, &ps);
	return 0;
}

static LRESULT popup_on_activate(FluxPopup *popup, WPARAM wp) {
	if (popup && popup->dismiss_on_outside && LOWORD(wp) == WA_INACTIVE) flux_popup_dismiss(popup);
	return 0;
}

static LRESULT popup_on_size(FluxPopup *popup, LPARAM lp) {
	if (!popup || !popup->graphics) return 0;

	int w = LOWORD(lp);
	int h = HIWORD(lp);
	if (w > 0 && h > 0) flux_graphics_resize(popup->graphics, w, h);
	return 0;
}

static bool popup_handle_mouse_message(PopupWindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_MOUSEMOVE   : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_MOVE); return true;
	case WM_LBUTTONDOWN : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_DOWN); return true;
	case WM_LBUTTONUP   : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_UP); return true;
	case WM_MOUSELEAVE  : *result = popup_on_mouse_leave(m->popup); return true;
	case WM_MOUSEWHEEL  : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_WHEEL); return true;
	default             : return false;
	}
}

static bool popup_handle_system_message(PopupWindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_PAINT         : *result = popup_on_paint(m->hwnd, m->popup); return true;
	case WM_ACTIVATE      : *result = popup_on_activate(m->popup, m->wp); return true;
	case WM_MOUSEACTIVATE : *result = MA_NOACTIVATE; return true;
	case WM_SIZE          : *result = popup_on_size(m->popup, m->lp); return true;
	default               : return false;
	}
}

static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	FluxPopup         *popup   = ( FluxPopup * ) ( LONG_PTR ) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	LRESULT            result  = 0;
	PopupWindowMessage message = {hwnd, popup, msg, wp, lp};

	if (popup_handle_mouse_message(&message, &result)) return result;
	if (popup_handle_system_message(&message, &result)) return result;

	return DefWindowProcW(hwnd, msg, wp, lp);
}
