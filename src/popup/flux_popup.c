#include "fluxent/flux_popup.h"
#include "graphics/flux_graphics_compose.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "popup/flux_popup_tree.h"
#include "render/flux_dl_replay.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"
#include "render/flux_scroll_geom.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct FluxPopupAnim {
	FluxPopupAnimStyle style;
	bool               active;
	bool               closing;
	bool               is_layered;
	float              progress;
	ULONGLONG          start_ms;
	DWORD              duration_ms;
	int                final_x, final_y, final_pw, final_ph;
	FluxPlacement      final_placement;
} FluxPopupAnim;

struct FluxPopup {
	FluxWindow           *owner;
	HWND                  owner_hwnd;
	HWND                  popup_hwnd;
	FluxGfx              *graphics;

	XentCtx              *ctx;
	FluxStore            *store;
	XentNodeId            content_root;
	FluxPopupTree        *tree;
	bool                  tree_busy;

	FluxRect              anchor;
	FluxPlacement         placement;
	float                 content_w;
	float                 content_h;

	bool                  is_visible;
	bool                  dismiss_on_outside;

	FluxPopupDismissFn    dismiss_cb;
	void                 *dismiss_ctx;

	FluxPopupPaintFn      paint_cb;
	void                 *paint_ctx;
	FluxPopupRecordFn     record_cb;
	void                 *record_ctx;
	FluxText             *record_text;
	ID2D1SolidColorBrush *record_brush;

	FluxPopupMouseFn      mouse_cb;
	void                 *mouse_ctx;
	bool                  mouse_tracking;
	bool                  last_input_touch;

	float                 max_content_h;

	bool                  acrylic_active;
	bool                  shadow;
	float                 split_closed;
	float                 split_offset;
	float                 corner;

	FluxPopupAnim         anim;
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

#define POPUP_MENU_DURATION_MS   250
#define POPUP_MENU_CLOSED_SCALE  0.5f
#define POPUP_FLYOUT_DURATION_MS 167
#define POPUP_FLYOUT_OFFSET_DIP  50.0f
#define POPUP_ACRYLIC_BLUR       30.0f

static void             popup_stop_animation(FluxPopup *popup);
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

	popup->graphics = flux_gfx_create(NULL);
	if (!popup->graphics) {
		DestroyWindow(popup->popup_hwnd);
		free(popup);
		return NULL;
	}

	HRESULT hr = flux_gfx_attach(popup->graphics, popup->popup_hwnd);
	if (FAILED(hr)) {
		flux_gfx_destroy(popup->graphics);
		DestroyWindow(popup->popup_hwnd);
		free(popup);
		return NULL;
	}

	return popup;
}

void flux_popup_destroy(FluxPopup *popup) {
	if (!popup) return;
	popup_registry_remove(popup);
	flux_anim_unregister(&popup->anim);
	flux_anim_unregister(&popup->tree_busy);
	if (popup->popup_hwnd) {
		SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, 0);
		DestroyWindow(popup->popup_hwnd);
	}
	flux_popup_tree_destroy(popup->tree);
	if (popup->record_brush) ID2D1SolidColorBrush_Release(popup->record_brush);
	if (popup->graphics) flux_gfx_destroy(popup->graphics);
	free(popup);
}

bool flux_popup_setcontent(FluxPopup *popup, XentCtx *ctx, FluxStore *store, XentNodeId content_root) {
	if (!popup || !ctx || !store || !xent_node_valid(ctx, content_root)) return false;
	FluxPopupTree *tree = flux_popup_tree_create(popup->owner, ctx, store, content_root);
	if (!tree) return false;
	flux_popup_tree_destroy(popup->tree);
	popup->tree         = tree;
	popup->ctx          = ctx;
	popup->store        = store;
	popup->content_root = content_root;
	return true;
}

void flux_popup_setsize(FluxPopup *popup, float width, float height) {
	if (!popup) return;
	popup->content_w = width;
	popup->content_h = height;
	if (popup->is_visible) popup_position(popup);
}

void flux_popup_setdismissoutside(FluxPopup *popup, bool dismiss) {
	if (!popup) return;
	popup->dismiss_on_outside = dismiss;
}

void flux_popup_ondismiss(FluxPopup *popup, FluxPopupDismissFn cb, void *ctx) {
	if (!popup) return;
	popup->dismiss_cb  = cb;
	popup->dismiss_ctx = ctx;
}

void flux_popup_setanim(FluxPopup *popup, FluxPopupAnimStyle style) {
	if (!popup) return;
	popup->anim.style = style;
}

static float popup_scale_for_owner(FluxPopup const *popup) {
	UINT dpi = GetDpiForWindow(popup->owner_hwnd);
	return dpi / FLUX_DPI_BASE;
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

static float popup_window_scale(HWND hwnd) {
	UINT dpi = GetDpiForWindow(hwnd);
	return dpi > 0 ? ( float ) dpi / FLUX_DPI_BASE : 1.0f;
}

typedef struct PopupInsets {
	float l, t, r, b;
} PopupInsets;

static PopupInsets popup_insets(FluxPopup const *popup) {
	if (!popup->shadow || !flux_graphics_composition_enabled(popup->graphics)) return (PopupInsets) {0};
	return (PopupInsets) {16.0f, 8.0f, 16.0f, 24.0f};
}

static FluxRect popup_split_band(FluxPopup const *popup);

static void     popup_draw_shadow(FluxPopup *popup, ID2D1DeviceContext *d2d) {
	PopupInsets in = popup_insets(popup);
	if (in.b <= 0.0f) return;
	ID2D1CommandList *caster = NULL;
	ID2D1Effect      *fx     = NULL;
	ID2D1Image       *target = NULL;
	if (FAILED(ID2D1DeviceContext_CreateCommandList(d2d, &caster))) return;
	ID2D1DeviceContext_GetTarget(d2d, &target);
	ID2D1DeviceContext_SetTarget(d2d, ( ID2D1Image * ) caster);
	FluxRect          band = popup_split_band(popup);
	D2D1_ROUNDED_RECT rr   = {
	  {in.l + band.x, in.t + band.y, in.l + band.x + band.w, in.t + band.y + band.h},
      popup->corner, popup->corner
    };
	ID2D1RenderTarget_FillRoundedRectangle(( ID2D1RenderTarget * ) d2d, &rr, ( ID2D1Brush * ) popup->record_brush);
	ID2D1CommandList_Close(caster);
	ID2D1DeviceContext_SetTarget(d2d, target);
	if (SUCCEEDED(ID2D1DeviceContext_CreateEffect(d2d, &CLSID_D2D1Shadow, &fx))) {
		bool  dark      = flux_theme_systemdark();
		float sigma     = 16.0f / 2.0f;
		float color [4] = {0.0f, 0.0f, 0.0f, dark ? 0.26f : 0.14f};
		ID2D1Effect_SetInput(fx, 0, ( ID2D1Image * ) caster, TRUE);
		ID2D1Effect_SetValue(
		  fx, D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, D2D1_PROPERTY_TYPE_FLOAT, ( UINT8 const * ) &sigma,
		  sizeof(sigma)
		);
		ID2D1Effect_SetValue(
		  fx, D2D1_SHADOW_PROP_COLOR, D2D1_PROPERTY_TYPE_VECTOR4, ( UINT8 const * ) color, sizeof(color)
		);
		ID2D1Image *out = NULL;
		ID2D1Effect_GetOutput(fx, &out);
		D2D_POINT_2F off = {0.0f, 8.0f};
		ID2D1DeviceContext_DrawImage(
		  d2d, out, &off, NULL, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER
		);
		if (out) (( IUnknown * ) out)->lpVtbl->Release(( IUnknown * ) out);
		(( IUnknown * ) fx)->lpVtbl->Release(( IUnknown * ) fx);
	}
	if (target) (( IUnknown * ) target)->lpVtbl->Release(( IUnknown * ) target);
	(( IUnknown * ) caster)->lpVtbl->Release(( IUnknown * ) caster);
}

void flux_popup_setsplit(FluxPopup *popup, float closed_length, float offset_from_center) {
	if (!popup) return;
	popup->split_closed = closed_length;
	popup->split_offset = offset_from_center;
}

static FluxRect popup_split_band(FluxPopup const *popup) {
	FluxRect full    = {0.0f, 0.0f, popup->content_w, popup->content_h};
	bool     clipped = popup->anim.style == FLUX_POPUP_ANIM_SPLIT || popup->anim.style == FLUX_POPUP_ANIM_PICKER;
	if (!clipped || !popup->anim.active || full.h <= 0.0f) return full;
	float t    = ( float ) (flux_anim_ms() - popup->anim.start_ms) / popup->anim.duration_ms;
	float e    = flux_cubic_bezier(t > 1.0f ? 1.0f : t, 0.0f, 0.0f, 0.0f, 1.0f);
	float off  = fabsf(popup->split_offset) / full.h;
	float from = popup->split_closed / full.h;
	float to   = (0.5f + off) * 2.0f;
	if (popup->anim.style == FLUX_POPUP_ANIM_PICKER) from = 0.5f + 2.0f * fmaxf(0.0f, off - 0.25f);
	if (popup->anim.closing) {
		from = to;
		to   = 0.15f + 2.0f * fmaxf(0.0f, off - 0.425f);
	}
	float h   = full.h * (from + (to - from) * e);
	float top = full.h * 0.5f + popup->split_offset - h * 0.5f;
	float y0  = top < 0.0f ? 0.0f : top;
	float y1  = top + h > full.h ? full.h : top + h;
	return (FluxRect) {0.0f, y0, full.w, y1 > y0 ? y1 - y0 : 0.0f};
}

void flux_popup_setshadow(FluxPopup *popup, bool shadow, float corner) {
	if (!popup) return;
	popup->shadow = shadow;
	popup->corner = corner;
}

FluxPoint flux_popup_contentorigin(FluxPopup const *popup) {
	RECT wr = {0};
	if (!popup || !popup->popup_hwnd || !GetWindowRect(popup->popup_hwnd, &wr)) return (FluxPoint) {0.0f, 0.0f};
	PopupInsets in    = popup_insets(popup);
	float       scale = popup_window_scale(popup->popup_hwnd);
	return (FluxPoint) {( float ) wr.left + in.l * scale, ( float ) wr.top + in.t * scale};
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

	PopupInsets in               = popup_insets(popup);
	x                           -= ( int ) (in.l * scale + 0.5f);
	y                           -= ( int ) (in.t * scale + 0.5f);
	pw                          += ( int ) ((in.l + in.r) * scale + 0.5f);
	ph                          += ( int ) ((in.t + in.b) * scale + 0.5f);

	popup->anim.final_x          = x;
	popup->anim.final_y          = y;
	popup->anim.final_pw         = pw;
	popup->anim.final_ph         = ph;
	popup->anim.final_placement  = params.placement;

	SetWindowPos(popup->popup_hwnd, HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);
	flux_gfx_resize(popup->graphics, pw, ph);
	if (popup->corner > 0.0f)
		flux_gfx_setplate(popup->graphics, in.l * scale, in.t * scale, aw * scale, ah * scale, popup->corner * scale);
}

static float popup_clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static float popup_menu_ease(float t) { return flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f); }

static float popup_flyout_ease(float t) { return flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f); }

static void  popup_apply_menu_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd) return;
	popup->anim.progress = popup_clampf(t, 0.0f, 1.0f);
	SetWindowPos(
	  popup->popup_hwnd, HWND_TOPMOST, popup->anim.final_x, popup->anim.final_y, popup->anim.final_pw,
	  popup->anim.final_ph, SWP_NOACTIVATE | SWP_NOREDRAW
	);
}

static void popup_apply_flyout_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd) return;

	UINT  dpi       = GetDpiForWindow(popup->owner_hwnd);
	float scale     = dpi > 0 ? dpi / FLUX_DPI_BASE : 1.0f;
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

static void popup_apply_open_frame(FluxPopup *popup, float t) {
	switch (popup->anim.style) {
	case FLUX_POPUP_ANIM_MENU    :
	case FLUX_POPUP_ANIM_SPLIT   :
	case FLUX_POPUP_ANIM_PICKER  : popup_apply_menu_frame(popup, popup_menu_ease(t)); break;
	case FLUX_POPUP_ANIM_FLYOUT  : popup_apply_flyout_frame(popup, popup_flyout_ease(t)); break;
	case FLUX_POPUP_ANIM_TOOLTIP : popup_apply_tooltip_frame(popup, t); break;
	case FLUX_POPUP_ANIM_NONE    :
	default                      : break;
	}
}

static DWORD popup_open_duration(FluxPopupAnimStyle style) {
	switch (style) {
	case FLUX_POPUP_ANIM_MENU   :
	case FLUX_POPUP_ANIM_SPLIT  :
	case FLUX_POPUP_ANIM_PICKER : return POPUP_MENU_DURATION_MS;
	case FLUX_POPUP_ANIM_FLYOUT : return POPUP_FLYOUT_DURATION_MS;
	default                     : return 0;
	}
}

static bool popup_anim_step(void *ctx, unsigned long now) {
	( void ) now;
	FluxPopup *popup = ( FluxPopup * ) (( char * ) ctx - offsetof(FluxPopup, anim));
	if (!popup->anim.active || popup->anim.duration_ms == 0) {
		popup->anim.active   = false;
		popup->anim.progress = 1.0f;
		return false;
	}
	float t    = ( float ) (flux_anim_ms() - popup->anim.start_ms) / ( float ) popup->anim.duration_ms;
	bool  done = t >= 1.0f;
	popup_apply_open_frame(popup, done ? 1.0f : t);
	if (done) {
		if (popup->anim.closing) ShowWindow(popup->popup_hwnd, SW_HIDE);
		popup->anim.active   = false;
		popup->anim.progress = 1.0f;
	}
	InvalidateRect(popup->popup_hwnd, NULL, FALSE);
	return !done;
}

static bool popup_tree_step(void *ctx, unsigned long now) {
	( void ) now;
	FluxPopup *popup = ( FluxPopup * ) (( char * ) ctx - offsetof(FluxPopup, tree_busy));
	if (popup->is_visible) InvalidateRect(popup->popup_hwnd, NULL, FALSE);
	return false;
}

static void popup_stop_animation(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd) return;
	flux_anim_unregister(&popup->anim);
	popup->anim.active   = false;
	popup->anim.progress = 1.0f;
}

static void popup_start_open_animation(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd) return;

	popup_stop_animation(popup);
	popup->anim.closing     = false;
	popup->anim.duration_ms = popup_open_duration(popup->anim.style);
	if (popup->anim.duration_ms == 0) {
		popup_apply_open_frame(popup, 1.0f);
		return;
	}

	popup->anim.active   = true;
	popup->anim.start_ms = flux_anim_ms();
	popup_apply_open_frame(popup, 0.0f);
	flux_anim_register(&popup->anim, popup_anim_step);
}

void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement) {
	if (!popup) return;

	popup->anchor     = anchor;
	popup->placement  = placement;
	popup->is_visible = true;

	popup_position(popup);

	popup_start_open_animation(popup);
	ShowWindow(popup->popup_hwnd, SW_SHOWNOACTIVATE);
	InvalidateRect(popup->popup_hwnd, NULL, FALSE);
}

static bool popup_start_close_animation(FluxPopup *popup) {
	if (popup->anim.style != FLUX_POPUP_ANIM_PICKER) return false;
	popup_stop_animation(popup);
	popup->anim.closing     = true;
	popup->anim.active      = true;
	popup->anim.start_ms    = flux_anim_ms();
	popup->anim.duration_ms = 167;
	flux_anim_register(&popup->anim, popup_anim_step);
	return true;
}

void flux_popup_dismiss(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;

	popup->is_visible = false;
	flux_anim_unregister(&popup->tree_busy);
	if (!popup_start_close_animation(popup)) {
		popup_stop_animation(popup);
		ShowWindow(popup->popup_hwnd, SW_HIDE);
	}
	if (popup->dismiss_cb) popup->dismiss_cb(popup->dismiss_ctx);
}

void flux_popup_reposition(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;
	popup_position(popup);
}

bool                    flux_popup_visible(FluxPopup const *popup) { return popup ? popup->is_visible : false; }

FluxPopupMenuTransition flux_popup_menutransition(FluxPopup const *popup, float opened_length) {
	FluxPopupMenuTransition tr = {
	  .active              = false,
	  .border_scale_y      = 1.0f,
	  .border_center_y     = 0.0f,
	  .content_translate_y = 0.0f,
	  .clip_translate_y    = 0.0f,
	};
	if (!popup || !popup->anim.active || popup->anim.style != FLUX_POPUP_ANIM_MENU) return tr;
	if (popup->anim.final_placement != FLUX_PLACEMENT_TOP && popup->anim.final_placement != FLUX_PLACEMENT_BOTTOM)
		return tr;

	float progress         = popup_clampf(popup->anim.progress, 0.0f, 1.0f);
	float remaining        = opened_length * POPUP_MENU_CLOSED_SCALE * (1.0f - progress);
	float direction        = popup->anim.final_placement == FLUX_PLACEMENT_BOTTOM ? 1.0f : -1.0f;

	tr.active              = true;
	tr.border_scale_y      = 1.0f - POPUP_MENU_CLOSED_SCALE * (1.0f - progress);
	tr.border_center_y     = popup->anim.final_placement == FLUX_PLACEMENT_TOP ? opened_length : 0.0f;
	tr.content_translate_y = direction * remaining;
	tr.clip_translate_y    = -tr.content_translate_y;
	return tr;
}

void flux_popup_onpaint(FluxPopup *popup, FluxPopupPaintFn cb, void *ctx) {
	if (!popup) return;
	popup->paint_cb    = cb;
	popup->paint_ctx   = ctx;
	popup->record_cb   = NULL;
	popup->record_ctx  = NULL;
	popup->record_text = NULL;
}

void flux_popup_onrecord(FluxPopup *popup, FluxPopupRecordFn cb, void *ctx, FluxText *text) {
	if (!popup) return;
	popup->record_cb   = cb;
	popup->record_ctx  = ctx;
	popup->record_text = text;
	popup->paint_cb    = NULL;
	popup->paint_ctx   = NULL;
}

void flux_popup_onmouse(FluxPopup *popup, FluxPopupMouseFn cb, void *ctx) {
	if (!popup) return;
	popup->mouse_cb  = cb;
	popup->mouse_ctx = ctx;
}

FluxGfx *flux_popup_gfx(FluxPopup *popup) { return popup ? popup->graphics : NULL; }

HWND     flux_popup_hwnd(FluxPopup *popup) { return popup ? popup->popup_hwnd : NULL; }

bool     flux_popup_lasttouch(FluxPopup const *popup) { return popup && popup->last_input_touch; }

bool     flux_popup_acrylicactive(FluxPopup const *popup) { return popup && popup->acrylic_active; }

#define POPUP_ACRYLIC_TINT_OPACITY 0.80f

FluxColor flux_popup_acrylictint(FluxPopup const *popup, FluxColor fill) {
	if (!popup || !popup->acrylic_active) return fill;
	uint32_t  a = ( uint32_t ) ((fill.rgba & 0xffu) * POPUP_ACRYLIC_TINT_OPACITY + 0.5f);
	FluxColor t = {(fill.rgba & 0xffffff00u) | (a & 0xffu)};
	return t;
}

void flux_popup_setmaxh(FluxPopup *popup, float max_h) {
	if (!popup) return;
	popup->max_content_h = (max_h > 0.0f) ? max_h : 0.0f;
	if (popup->is_visible) popup_position(popup);
}

bool flux_popup_lightdismissvisible(HWND owner_hwnd) {
	if (!owner_hwnd) return false;
	for (int i = 0; i < g_popup_registry_count; i++) {
		FluxPopup *p = g_popup_registry [i];
		if (p && p->is_visible && p->dismiss_on_outside && p->owner_hwnd == owner_hwnd) return true;
	}
	return false;
}

void flux_popup_dismissowner(HWND owner_hwnd) {
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

	popup->last_input_touch = (GetMessageExtraInfo() & 0xffffff00) == 0xff515700;

	if (ev == FLUX_POPUP_MOUSE_MOVE) popup_begin_mouse_tracking(m->hwnd, popup);

	PopupMouseCoord point
	  = ev == FLUX_POPUP_MOUSE_WHEEL ? popup_wheel_point(m->hwnd, m->wp, m->lp) : popup_mouse_point(m->hwnd, m->lp);
	PopupInsets in  = popup_insets(popup);
	point.x        -= in.l;
	if (ev != FLUX_POPUP_MOUSE_WHEEL) point.y -= in.t;
	popup->mouse_cb(popup->mouse_ctx, popup, ev, point.x, point.y);
	return 0;
}

static bool popup_on_pointer(PopupWindowMessage const *m) {
	UINT32             pid  = GET_POINTERID_WPARAM(m->wp);
	POINTER_INPUT_TYPE type = PT_POINTER;
	if (!GetPointerType(pid, &type) || type == PT_MOUSE || type == PT_TOUCHPAD) return false;
	FluxPopup *popup = m->popup;
	if (!popup || !popup->mouse_cb || !IS_POINTER_PRIMARY_WPARAM(m->wp)) return true;
	if (m->msg == WM_POINTERUPDATE && !IS_POINTER_INCONTACT_WPARAM(m->wp)) return true;
	FluxPopupMouseEvent ev = m->msg == WM_POINTERDOWN ? FLUX_POPUP_MOUSE_DOWN
	                       : m->msg == WM_POINTERUP   ? FLUX_POPUP_MOUSE_UP
	                                                  : FLUX_POPUP_MOUSE_MOVE;
	POINT               pt = {( short ) LOWORD(m->lp), ( short ) HIWORD(m->lp)};
	ScreenToClient(m->hwnd, &pt);
	float       scale       = popup_window_scale(m->hwnd);
	PopupInsets in          = popup_insets(popup);
	popup->last_input_touch = true;
	popup->mouse_cb(popup->mouse_ctx, popup, ev, ( float ) pt.x / scale - in.l, ( float ) pt.y / scale - in.t);
	return true;
}

static LRESULT popup_on_mouse_leave(FluxPopup *popup) {
	if (!popup) return 0;

	popup->mouse_tracking = false;
	if (popup->mouse_cb) popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_LEAVE, -1.0f, -1.0f);
	return 0;
}

static bool popup_ensure_record_brush(FluxPopup *popup, ID2D1DeviceContext *d2d) {
	if (popup->record_brush) return true;
	D2D1_COLOR_F          black = {0.0f, 0.0f, 0.0f, 1.0f};
	D2D1_BRUSH_PROPERTIES props = flux_default_brush_props();
	HRESULT               hr
	  = ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &props, &popup->record_brush);
	return SUCCEEDED(hr);
}

static void popup_replay_record(FluxPopup *popup) {
	ID2D1DeviceContext *d2d = flux_gfx_d2d(popup->graphics);
	if (!d2d || !popup_ensure_record_brush(popup, d2d)) return;
	XentDl *list = popup->record_cb(popup->record_ctx, popup);
	if (!list) return;
	FluxRenderCtx rc = {
	  .d2d   = d2d,
	  .brush = popup->record_brush,
	  .text  = popup->record_text,
	};
	( void ) flux_xtk_replay_rc(&rc, list);
	xent_dl_free(list);
}

static void popup_split_plate(FluxPopup *popup, FluxRect const *band) {
	if (popup->corner <= 0.0f) return;
	PopupInsets in = popup_insets(popup);
	float       s  = popup_window_scale(popup->popup_hwnd);
	flux_gfx_setplate(
	  popup->graphics, (in.l + band->x) * s, (in.t + band->y) * s, band->w * s, band->h * s, popup->corner * s
	);
}

static void popup_paint_tree(FluxPopup *popup) {
	ID2D1DeviceContext *d2d = flux_gfx_d2d(popup->graphics);
	popup->tree_busy        = false;
	if (d2d && popup_ensure_record_brush(popup, d2d)) {
		FluxRenderCtx rc = {.d2d = d2d, .brush = popup->record_brush};
		popup->tree_busy = flux_popup_tree_paint(popup->tree, popup, &rc);
	}
	if (popup->tree_busy && popup->is_visible) flux_anim_register(&popup->tree_busy, popup_tree_step);
	else flux_anim_unregister(&popup->tree_busy);
}

static void popup_run_paint(FluxPopup *popup) {
	if (popup->record_cb) {
		popup_replay_record(popup);
		return;
	}
	if (popup->paint_cb) {
		popup->paint_cb(popup->paint_ctx, popup);
		return;
	}
	if (popup->tree) popup_paint_tree(popup);
}

static void popup_paint_content(FluxPopup *popup) {
	ID2D1DeviceContext *d2d = flux_gfx_d2d(popup->graphics);
	if (d2d && popup_ensure_record_brush(popup, d2d)) popup_draw_shadow(popup, d2d);
	PopupInsets in    = popup_insets(popup);
	FluxRect    band  = popup_split_band(popup);
	bool        split = d2d && band.h < popup->content_h;
	if (popup->anim.style == FLUX_POPUP_ANIM_SPLIT || popup->anim.style == FLUX_POPUP_ANIM_PICKER)
		popup_split_plate(popup, &band);
	D2D1_MATRIX_3X2_F m = {1.0f, 0.0f, 0.0f, 1.0f, in.l, in.t};
	if (d2d) ID2D1RenderTarget_SetTransform(( ID2D1RenderTarget * ) d2d, &m);
	D2D1_RECT_F clip = {band.x, band.y, band.x + band.w, band.y + band.h};
	if (split) ID2D1RenderTarget_PushAxisAlignedClip(( ID2D1RenderTarget * ) d2d, &clip, D2D1_ANTIALIAS_MODE_ALIASED);
	popup_run_paint(popup);
	if (split) ID2D1RenderTarget_PopAxisAlignedClip(( ID2D1RenderTarget * ) d2d);
	m.dx = 0.0f;
	m.dy = 0.0f;
	if (d2d) ID2D1RenderTarget_SetTransform(( ID2D1RenderTarget * ) d2d, &m);
}

static float popup_picker_opacity(FluxPopup const *popup) {
	if (popup->anim.style != FLUX_POPUP_ANIM_PICKER || !popup->anim.closing || !popup->anim.active) return 1.0f;
	float elapsed = ( float ) (flux_anim_ms() - popup->anim.start_ms);
	return 1.0f - popup_clampf((elapsed - 84.0f) / 83.0f, 0.0f, 1.0f);
}

static LRESULT popup_on_paint(HWND hwnd, FluxPopup *popup) {
	PAINTSTRUCT ps;
	BeginPaint(hwnd, &ps);
	if (!popup || !popup->graphics) {
		EndPaint(hwnd, &ps);
		return 0;
	}

	popup->acrylic_active = flux_gfx_enable_acrylic(popup->graphics, POPUP_ACRYLIC_BLUR);

	flux_gfx_setopacity(popup->graphics, popup_picker_opacity(popup));
	flux_gfx_begin(popup->graphics);
	flux_gfx_clear(popup->graphics, flux_color_rgba(0, 0, 0, 0));

	popup_paint_content(popup);

	flux_gfx_end(popup->graphics);
	flux_gfx_present(popup->graphics, true);
	flux_gfx_commit(popup->graphics);
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
	if (w > 0 && h > 0) flux_gfx_resize(popup->graphics, w, h);
	return 0;
}

static bool popup_handle_pointer_message(PopupWindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_POINTERDOWN     :
	case WM_POINTERUPDATE   :
	case WM_POINTERUP       : *result = 0; return popup_on_pointer(m);
	case WM_POINTERACTIVATE : *result = PA_NOACTIVATE; return true;
	default                 : return false;
	}
}

static bool popup_handle_mouse_message(PopupWindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_MOUSEMOVE   : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_MOVE); return true;
	case WM_LBUTTONDOWN : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_DOWN); return true;
	case WM_LBUTTONUP   : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_UP); return true;
	case WM_MOUSELEAVE  : *result = popup_on_mouse_leave(m->popup); return true;
	case WM_MOUSEWHEEL  : *result = popup_on_mouse_event(m, FLUX_POPUP_MOUSE_WHEEL); return true;
	default             : return popup_handle_pointer_message(m, result);
	}
}

static LRESULT popup_on_hittest(PopupWindowMessage const *m) {
	if (!m->popup) return HTCLIENT;
	POINT pt = {( short ) LOWORD(m->lp), ( short ) HIWORD(m->lp)};
	ScreenToClient(m->hwnd, &pt);
	PopupInsets in    = popup_insets(m->popup);
	float       scale = popup_window_scale(m->hwnd);
	float       x = pt.x / scale - in.l, y = pt.y / scale - in.t;
	bool        inside = x >= 0.0f && y >= 0.0f && x < m->popup->content_w && y < m->popup->content_h;
	return inside ? HTCLIENT : HTTRANSPARENT;
}

static bool popup_handle_system_message(PopupWindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_PAINT         : *result = popup_on_paint(m->hwnd, m->popup); return true;
	case WM_ACTIVATE      : *result = popup_on_activate(m->popup, m->wp); return true;
	case WM_MOUSEACTIVATE : *result = MA_NOACTIVATE; return true;
	case WM_SIZE          : *result = popup_on_size(m->popup, m->lp); return true;
	case WM_NCHITTEST     : *result = popup_on_hittest(m); return true;
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
