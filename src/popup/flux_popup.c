#include "fluxent/flux_popup.h"

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
  #define DWMSBT_TRANSIENTWINDOW 3 /* Acrylic ("Menu") */
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

/* ------- Popup struct ------- */

struct FluxPopup {
	FluxWindow              *owner;
	HWND                     owner_hwnd;
	HWND                     popup_hwnd;
	FluxGraphics            *graphics;

	XentContext             *ctx;
	FluxNodeStore           *store;
	XentNodeId               content_root;

	FluxRect                 anchor; /* screen coords */
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
	bool                     mouse_tracking; /* TrackMouseEvent active? */

	/* System backdrop state */
	bool                     backdrop_enabled; /* DWM transient acrylic active? */

	/* Clamp */
	float                    max_content_h; /* 0 = no clamp */

	/* ── Entrance animation ─────────────────────────────────────── */
	FluxPopupAnimStyle       anim_style; /* MENU / FLYOUT / TOOLTIP / NONE */
	bool                     anim_active;
	DWORD                    anim_start_tick;
	LARGE_INTEGER            anim_start_qpc;   /* high-res timestamp (sub-ms) */
	UINT_PTR                 anim_timer_id;
	float                    anim_offset_y;    /* initial Y offset in DIPs (negative = from above) */
	float                    anim_duration_ms; /* total duration */
	bool                     is_layered;       /* WS_EX_LAYERED applied? */

	/* Final (fully-open) window rect in screen pixels + resolved
	   placement — cached by popup_position so the WM_TIMER handler
	   can shrink/grow the HWND in sync with the D2D ScaleY transform.
	   Without this, DWM's system-backdrop fills the full HWND rect
	   while the D2D content scales inside, producing an empty dark
	   canvas outside the scaled content. */
	int                      final_x, final_y, final_pw, final_ph;
	FluxPlacement            final_placement;
};

/* ── Popup registry (for owner-window interruption) ────────── */
#define FLUX_POPUP_REGISTRY_MAX 64
static FluxPopup *g_popup_registry [FLUX_POPUP_REGISTRY_MAX];
static int        g_popup_registry_count = 0;

static void       popup_registry_add(FluxPopup *p) {
	if (!p) return;
	for (int i = 0; i < g_popup_registry_count; i++)
		if (g_popup_registry [i] == p) return;
	if (g_popup_registry_count < FLUX_POPUP_REGISTRY_MAX) g_popup_registry [g_popup_registry_count++] = p;
}

static void popup_registry_remove(FluxPopup *p) {
		for (int i = 0; i < g_popup_registry_count; i++) {
				if (g_popup_registry [i] == p) {
					g_popup_registry [i] = g_popup_registry [--g_popup_registry_count];
					return;
				}
		}
}

/* ── Animation constants — WinUI 3 MenuPopupThemeTransition ─────────
 *
 * Sourced verbatim from microsoft-ui-xaml:
 *   src/dxaml/xcp/dxaml/lib/MenuPopupThemeTransition_Partial.h
 *       s_OpenDuration           = 250 ms
 *       s_OpacityChangeDuration  = 83  ms  (close-fade only)
 *   src/dxaml/xcp/dxaml/lib/LayoutTransition_partial.cpp
 *       CreateStoryboardImpl (TransitionTrigger_Load):
 *           easing   = Cubic-Bezier(0, 0, 0, 1)    (cp3.X = 0)
 *           opacity  = (none — menu does NOT fade on open)
 *           border ScaleY : (1 - closedRatio) → 1.0
 *           border CenterY: openedLength  when direction==Top
 *                           0             when direction==Bottom
 *           content TranslateY       : ±L·closedRatio → 0
 *           content ClipTranslateY   : ∓L·closedRatio → 0
 *   src/dxaml/xcp/dxaml/lib/MenuFlyout_Partial.cpp  L253:
 *       closedRatio = 0.5
 *       direction = (placement==Top) ? Bottom : Top
 *
 * Because fluxent paints the popup (border + content) as a single
 * Direct2D surface, we collapse the equivalent combination of
 * (content TranslateY) + (clip ClipTranslateY) + (border ScaleY
 * origin=anchor-edge) into the net visible effect:
 *
 *     ScaleY : 0.5 → 1.0  with origin at the anchor edge
 *     (no opacity, no translate on open)
 *
 * Mathematically identical in world-space: the anchor edge stays
 * pinned while the far edge expands from 50% to 100% height.         */
#define POPUP_ANIM_DURATION_MS       250.0f
#define POPUP_ANIM_CLOSE_DURATION_MS 83.0f
#define POPUP_ANIM_SCALE_FROM        0.5f
#define POPUP_ANIM_TIMER_ID          42
#define POPUP_ANIM_INTERVAL_MS         \
	8 /* ~120 Hz — oversample so the \
	     animation doesn't alias with  \
	     display vblank at any refresh \
	     rate up to 120 Hz */

/* PopupThemeTransition (base, used by Flyout/FlyoutPresenter/CommandBarFlyout).
 * Values taken from ThemeGenerator TAS_SHOWPOPUP OS msstyles:
 *   translate offset  : 50 DIP along placement axis
 *   translate duration: 167 ms, ease-out
 *   opacity           : 0 → 1, linear, 83 ms
 *
 * We match the visible outer duration (167 ms) and apply opacity over
 * the same span for simplicity; the OS curve is very close to the
 * MenuFlyout bezier (0,0,0,1) so we reuse popup_ease_cp3x0.            */
#define POPUP_FLYOUT_DURATION_MS  167.0f
#define POPUP_FLYOUT_OFFSET_DIP   50.0f

/* Fade{In,Out}ThemeAnimation (ToolTip). */
#define POPUP_TOOLTIP_DURATION_MS 83.0f

/* Exact Cubic-Bezier(0, 0, 0, 1) as used by WinUI's
 * TimingFunctionDescription when cp3.X = 0.
 *
 * Derivation: with control points P0=(0,0), P1=(0,0), P2=(0,1), P3=(1,1)
 *   x(t) = 3(1-t)t²·0 + 3(1-t)²t·0 + t³·1          = t³
 *   y(t) = 3(1-t)t²·1 + t³                         = 3t² − 2t³
 * Since x = t³, t = x^(1/3), giving the closed-form
 *   y(x) = 3·x^(2/3) − 2·x
 *
 * This is noticeably more front-loaded than the textbook ease-out-cubic
 * (1 − (1 − x)³): at x=0.3 we get y=0.743 vs 0.657 — an 8% delta that
 * shows up as a visible acceleration mismatch if you approximate. */
static float inline popup_ease_cp3x0(float x) {
	if (x <= 0.0f) return 0.0f;
	if (x >= 1.0f) return 1.0f;
	/* powf(x, 2/3) = cbrtf(x*x) */
	float x23 = cbrtf(x * x);
	return 3.0f * x23 - 2.0f * x;
}

static float inline popup_elapsed_ms_qpc(const LARGE_INTEGER *start) {
	LARGE_INTEGER now, freq;
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&freq);
	return ( float ) (( double ) (now.QuadPart - start->QuadPart) * 1000.0 / ( double ) freq.QuadPart);
}

/* ------- Forward declarations ------- */
static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void             popup_position(FluxPopup *popup);
static wchar_t const   *POPUP_CLASS_NAME = L"FluxPopupClass";
static ATOM             s_popup_class    = 0;

/* ------- Register window class (once) ------- */
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

/* ------- Create / Destroy ------- */

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

	/* Create the popup HWND (initially hidden) */
	popup->popup_hwnd         = CreateWindowExW(
	  WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, POPUP_CLASS_NAME, L"", WS_POPUP | WS_CLIPSIBLINGS, 0, 0, 1, 1,
	  popup->owner_hwnd, NULL, GetModuleHandleW(NULL), popup
	);

		if (!popup->popup_hwnd) {
			free(popup);
			return NULL;
		}

	SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, ( LONG_PTR ) popup);

	popup_registry_add(popup);

	/* Create D2D graphics for the popup */
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

/* ------- Content ------- */

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

/* Add WS_EX_LAYERED so we can animate per-window alpha via
 * SetLayeredWindowAttributes. Safe to call repeatedly; no-op once set.
 * DirectComposition + DWM system backdrop both continue to work under
 * LWA_ALPHA — the OS composites the entire window (backdrop + DComp
 * tree) at the given alpha, which is exactly what the WinUI transition
 * does for FadeIn / opacity-keyframes on the popup root visual.        */
static void popup_ensure_layered(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd || popup->is_layered) return;
	LONG_PTR ex = GetWindowLongPtrW(popup->popup_hwnd, GWL_EXSTYLE);
	SetWindowLongPtrW(popup->popup_hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
	/* Seed the alpha to fully opaque so non-animated code paths still
	 * render correctly (the attribute sticks after the style change). */
	SetLayeredWindowAttributes(popup->popup_hwnd, 0, 255, LWA_ALPHA);
	popup->is_layered = true;
}

void flux_popup_set_anim_style(FluxPopup *popup, FluxPopupAnimStyle style) {
	if (!popup) return;
	popup->anim_style = style;
	/* FLYOUT and TOOLTIP both animate window opacity, so upgrade to a
	 * layered window once. MENU and NONE don't need it.                 */
	if (style == FLUX_POPUP_ANIM_FLYOUT || style == FLUX_POPUP_ANIM_TOOLTIP) popup_ensure_layered(popup);
}

/* ------- Position calculation with flip logic ------- */

static void popup_position(FluxPopup *popup) {
	if (!popup || !popup->popup_hwnd) return;

	float       aw    = popup->content_w;
	float       ah    = popup->content_h;

	/* Get DPI scale */
	UINT        dpi   = GetDpiForWindow(popup->owner_hwnd);
	float       scale = dpi / 96.0f;

	/* Get monitor work area for flip logic */
	HMONITOR    mon   = MonitorFromWindow(popup->owner_hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi    = {.cbSize = sizeof(mi)};
	GetMonitorInfoW(mon, &mi);
	RECT work = mi.rcWork;

	/* Clamp the popup's height to the monitor work area (minus a small
	   margin) so a long menu never extends beyond the visible screen.
	   If the caller supplied a stricter max via
	   flux_popup_set_max_content_height(), honour that too.              */
	{
		float const SCREEN_MARGIN_DIP = 8.0f;
		float       work_h_dip        = (( float ) (work.bottom - work.top)) / scale - 2.0f * SCREEN_MARGIN_DIP;
		if (work_h_dip < 64.0f) work_h_dip = 64.0f;
		if (popup->max_content_h > 0.0f && popup->max_content_h < work_h_dip) work_h_dip = popup->max_content_h;
		if (ah > work_h_dip) ah = work_h_dip;
	}

	int           pw = ( int ) (aw * scale + 0.5f);
	int           ph = ( int ) (ah * scale + 0.5f);

	FluxRect      a  = popup->anchor;
	int           x = 0, y = 0;
	FluxPlacement pl = popup->placement;

		/* Try placement, flip if it goes off screen */
		for (int attempt = 0; attempt < 2; attempt++) {
				switch (pl) {
				case FLUX_PLACEMENT_BOTTOM :
					/* When anchor has real dimensions (control-anchored flyout):
					   center horizontally and add a 4 DIP gap below.
					   When anchor is a point (cursor-positioned context menu):
					   place top-left at the point with no gap. */
					if (a.w > 0.5f) x = ( int ) (a.x + (a.w - aw * scale) * 0.5f);
					else x = ( int ) (a.x);
					if (a.w > 0.5f || a.h > 0.5f) y = ( int ) (a.y + a.h + 4 * scale);
					else y = ( int ) (a.y + a.h);
						if (y + ph > work.bottom && attempt == 0) {
							pl = FLUX_PLACEMENT_TOP;
							continue;
						}
					break;
				case FLUX_PLACEMENT_TOP :
					if (a.w > 0.5f) x = ( int ) (a.x + (a.w - aw * scale) * 0.5f);
					else x = ( int ) (a.x);
					if (a.w > 0.5f || a.h > 0.5f) y = ( int ) (a.y - ph - 4 * scale);
					else y = ( int ) (a.y - ph);
						if (y < work.top && attempt == 0) {
							pl = FLUX_PLACEMENT_BOTTOM;
							continue;
						}
					break;
				case FLUX_PLACEMENT_RIGHT :
					x = ( int ) (a.x + a.w + (a.w > 0.5f ? 4 * scale : 0));
					y = ( int ) (a.y + (a.h - ah * scale) * 0.5f);
						if (x + pw > work.right && attempt == 0) {
							pl = FLUX_PLACEMENT_LEFT;
							continue;
						}
					break;
				case FLUX_PLACEMENT_LEFT :
					x = ( int ) (a.x - pw - (a.w > 0.5f ? 4 * scale : 0));
					y = ( int ) (a.y + (a.h - ah * scale) * 0.5f);
						if (x < work.left && attempt == 0) {
							pl = FLUX_PLACEMENT_RIGHT;
							continue;
						}
					break;
				case FLUX_PLACEMENT_AUTO :
				default                  : pl = FLUX_PLACEMENT_BOTTOM; continue;
				}
			break;
		}

	/* Clamp to work area */
	if (x + pw > work.right) x = work.right - pw;
	if (x < work.left) x = work.left;
	if (y + ph > work.bottom) y = work.bottom - ph;
	if (y < work.top) y = work.top;

	/* Cache final rect + resolved placement for animation_tick. */
	popup->final_x         = x;
	popup->final_y         = y;
	popup->final_pw        = pw;
	popup->final_ph        = ph;
	popup->final_placement = pl;

	SetWindowPos(popup->popup_hwnd, HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);

	flux_graphics_resize(popup->graphics, pw, ph);
}

/* Resize/reposition the HWND during the entrance animation so the DWM
 * system-backdrop tracks the D2D ScaleY transform exactly. Without this
 * the backdrop fills the full final rect while the scaled content
 * occupies only a fraction of it, producing a visible empty dark canvas.
 *
 * scaleY is the instantaneous ScaleY value from the easing curve (0.5→1.0
 * for MenuPopupThemeTransition). For BOTTOM placement (direction=Top in
 * WinUI) the window's top edge stays pinned to the anchor; for TOP
 * placement the bottom edge stays pinned. */
static void popup_apply_animated_rect(FluxPopup *popup, float scaleY) {
	if (!popup || !popup->popup_hwnd) return;
	if (popup->final_ph <= 0) return;

	int cur_ph = ( int ) (( float ) popup->final_ph * scaleY + 0.5f);
	if (cur_ph < 1) cur_ph = 1;

	int cur_y = popup->final_y;
		switch (popup->final_placement) {
		case FLUX_PLACEMENT_TOP :
			/* Bottom pinned: top shifts down as height shrinks. */
			cur_y = popup->final_y + (popup->final_ph - cur_ph);
			break;
		case FLUX_PLACEMENT_BOTTOM :
		case FLUX_PLACEMENT_LEFT :
		case FLUX_PLACEMENT_RIGHT :
		case FLUX_PLACEMENT_AUTO :
		default :
			/* Top pinned (MenuFlyout default for placement=BOTTOM). */
			break;
		}

	/* Note: we deliberately do NOT call flux_graphics_resize() here.
	 * The DirectComposition swap chain stays at the FINAL buffer size
	 * (created once in popup_position) so there is no per-tick
	 * ResizeBuffers → CreateBitmapFromDxgiSurface thrash. The HWND
	 * clip region (enforced by the DComp target) crops the visual
	 * content to the shrunken rect, which is exactly the behaviour
	 * we want. Doing it this way eliminates the single biggest source
	 * of animation judder. */
	SetWindowPos(
	  popup->popup_hwnd, HWND_TOPMOST, popup->final_x, cur_y, popup->final_pw, cur_ph, SWP_NOACTIVATE | SWP_NOREDRAW
	);
}

/* PopupThemeTransition: translate along placement axis + opacity.
 * `t` is the eased progress 0→1.                                      */
static void popup_apply_flyout_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd) return;

	UINT  dpi       = GetDpiForWindow(popup->owner_hwnd);
	float scale     = dpi > 0 ? dpi / 96.0f : 1.0f;
	int   offset_px = ( int ) (POPUP_FLYOUT_OFFSET_DIP * scale + 0.5f);

	/* Direction per WinUI: placement axis, signed so content slides
	 * INTO its final position (fromX,fromY)→(0,0):
	 *   BOTTOM (content below anchor)  → starts  above (−Y)
	 *   TOP    (content above anchor)  → starts  below (+Y)
	 *   RIGHT  (content right of anchor) → starts left (−X)
	 *   LEFT   (content left of anchor)  → starts right (+X)            */
	int   dx = 0, dy = 0;
	int   from = ( int ) ((1.0f - t) * ( float ) offset_px + 0.5f);
		switch (popup->final_placement) {
		case FLUX_PLACEMENT_BOTTOM : dy = -from; break;
		case FLUX_PLACEMENT_TOP    : dy = +from; break;
		case FLUX_PLACEMENT_RIGHT  : dx = -from; break;
		case FLUX_PLACEMENT_LEFT   : dx = +from; break;
		default                    : break;
		}

	SetWindowPos(
	  popup->popup_hwnd, HWND_TOPMOST, popup->final_x + dx, popup->final_y + dy, popup->final_pw, popup->final_ph,
	  SWP_NOACTIVATE | SWP_NOREDRAW
	);

	/* Opacity: linear 0→1, but we drive it off the same eased `t` so
	 * it stays in lock-step with the translate; visually equivalent
	 * to the WinUI fade since the fade duration (83 ms) fits entirely
	 * within the 167 ms translate window.                              */
	BYTE alpha = ( BYTE ) (t * 255.0f + 0.5f);
	if (popup->is_layered) SetLayeredWindowAttributes(popup->popup_hwnd, 0, alpha, LWA_ALPHA);
}

/* FadeInThemeAnimation (ToolTip): opacity only, linear, 83 ms.        */
static void popup_apply_tooltip_frame(FluxPopup *popup, float t) {
	if (!popup || !popup->popup_hwnd || !popup->is_layered) return;
	BYTE alpha = ( BYTE ) (t * 255.0f + 0.5f);
	SetLayeredWindowAttributes(popup->popup_hwnd, 0, alpha, LWA_ALPHA);
}

/* ------- Show / Dismiss ------- */

void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement) {
	if (!popup) return;

	popup->anchor     = anchor;
	popup->placement  = placement;
	popup->is_visible = true;

	popup_position(popup);

	/* Start entrance animation — dispatch on anim_style. */
	popup->anim_active     = true;
	popup->anim_start_tick = GetTickCount();
	QueryPerformanceCounter(&popup->anim_start_qpc);

		switch (popup->anim_style) {
		case FLUX_POPUP_ANIM_FLYOUT :
			popup->anim_duration_ms = POPUP_FLYOUT_DURATION_MS;
			/* Initial frame: translated + fully transparent. */
			popup_apply_flyout_frame(popup, 0.0f);
			break;
		case FLUX_POPUP_ANIM_TOOLTIP :
			popup->anim_duration_ms = POPUP_TOOLTIP_DURATION_MS;
			popup_apply_tooltip_frame(popup, 0.0f);
			break;
		case FLUX_POPUP_ANIM_NONE :
			popup->anim_duration_ms = 0.0f;
			popup->anim_active      = false;
			break;
		case FLUX_POPUP_ANIM_MENU :
		default :
			popup->anim_duration_ms = POPUP_ANIM_DURATION_MS;
			/* Pre-shrink the HWND to the initial 0.5× height BEFORE showing
			 * it, so the first visible frame already matches the scaled D2D
			 * content and there is no 1-frame flash of the full-size dark
			 * canvas.                                                       */
			popup_apply_animated_rect(popup, POPUP_ANIM_SCALE_FROM);
			break;
		}

	if (popup->anim_active)
		popup->anim_timer_id = SetTimer(popup->popup_hwnd, POPUP_ANIM_TIMER_ID, POPUP_ANIM_INTERVAL_MS, NULL);

	ShowWindow(popup->popup_hwnd, SW_SHOWNOACTIVATE);
	InvalidateRect(popup->popup_hwnd, NULL, FALSE);
}

void flux_popup_dismiss(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;

	popup->is_visible = false;

		/* Kill animation timer if still running */
		if (popup->anim_timer_id) {
			KillTimer(popup->popup_hwnd, popup->anim_timer_id);
			popup->anim_timer_id = 0;
		}
	popup->anim_active = false;

	ShowWindow(popup->popup_hwnd, SW_HIDE);

	if (popup->dismiss_cb) popup->dismiss_cb(popup->dismiss_ctx);
}

void flux_popup_update_position(FluxPopup *popup) {
	if (!popup || !popup->is_visible) return;
	popup_position(popup);
}

bool flux_popup_is_visible(FluxPopup const *popup) { return popup ? popup->is_visible : false; }

/* ------- Paint callback & accessors ------- */

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

/* ------- System backdrop / clamp ------- */

bool          flux_popup_enable_system_backdrop(FluxPopup *popup, bool is_dark) {
	if (!popup || !popup->popup_hwnd) return false;

	/* 1) Immersive dark-mode flag — the DWM uses this to pick the dark
	 *    or light acrylic tint. Must be set BEFORE the backdrop type
	 *    so the first frame composited by DWM already uses the correct
	 *    tint.                                                         */
	BOOL dark = is_dark ? TRUE : FALSE;
	DwmSetWindowAttribute(popup->popup_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	/* 2) Rounded window corners. Without this the DWM acrylic fills the
	 *    entire rectangular HWND — our D2D rounded-rect card leaves the
	 *    four corners "outside" the card painted with acrylic, which
	 *    looks like a square popup. DWMWCP_ROUND matches an 8 DIP
	 *    radius (OverlayCornerRadius).                                  */
	int corner = DWMWCP_ROUND;
	DwmSetWindowAttribute(popup->popup_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

	/* 3) Request DWM transient-window acrylic (Windows 11 22H2+).      */
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
	/* Snapshot: dismiss_cb may destroy popups, which mutates the registry.
	 * Copy the matching list first, then dismiss from the copy.          */
	FluxPopup *to_dismiss [FLUX_POPUP_REGISTRY_MAX];
	int        n = 0;
		for (int i = 0; i < g_popup_registry_count; i++) {
			FluxPopup *p = g_popup_registry [i];
			if (p && p->is_visible && p->owner_hwnd == owner_hwnd) to_dismiss [n++] = p;
		}
	for (int i = 0; i < n; i++) flux_popup_dismiss(to_dismiss [i]);
}

/* ------- Window proc ------- */

static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	FluxPopup *popup = ( FluxPopup * ) ( LONG_PTR ) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

		switch (msg) {

			/* ---- Mouse events (for MenuFlyout item tracking) ---- */
			case WM_MOUSEMOVE : {
					if (popup && popup->mouse_cb) {
							/* Ensure we get WM_MOUSELEAVE when the cursor exits */
							if (!popup->mouse_tracking) {
								TRACKMOUSEEVENT tme;
								tme.cbSize      = sizeof(tme);
								tme.dwFlags     = TME_LEAVE;
								tme.hwndTrack   = hwnd;
								tme.dwHoverTime = 0;
								TrackMouseEvent(&tme);
								popup->mouse_tracking = true;
							}
						/* WM_MOUSEMOVE delivers physical-pixel coords, but the
						   paint callback (D2D) works in logical/DIP coords.
						   Divide by the DPI scale so hit-testing matches painting. */
						UINT  dpi   = GetDpiForWindow(hwnd);
						float scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;
						float x     = ( float ) ( short ) LOWORD(lp) / scale;
						float y     = ( float ) ( short ) HIWORD(lp) / scale;
						popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_MOVE, x, y);
					}
				return 0;
			}

			case WM_LBUTTONDOWN : {
					if (popup && popup->mouse_cb) {
						UINT  dpi   = GetDpiForWindow(hwnd);
						float scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;
						float x     = ( float ) ( short ) LOWORD(lp) / scale;
						float y     = ( float ) ( short ) HIWORD(lp) / scale;
						popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_DOWN, x, y);
					}
				return 0;
			}

			case WM_LBUTTONUP : {
					if (popup && popup->mouse_cb) {
						UINT  dpi   = GetDpiForWindow(hwnd);
						float scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;
						float x     = ( float ) ( short ) LOWORD(lp) / scale;
						float y     = ( float ) ( short ) HIWORD(lp) / scale;
						popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_UP, x, y);
					}
				return 0;
			}

			case WM_MOUSELEAVE : {
					if (popup) {
						popup->mouse_tracking = false;
						if (popup->mouse_cb)
							popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_LEAVE, -1.0f, -1.0f);
					}
				return 0;
			}

			case WM_MOUSEWHEEL : {
					/* WM_MOUSEWHEEL is sent to the focused/captured window. Because
				     * our popup is WS_EX_NOACTIVATE, the OS sends the wheel to the
				     * cursor-under window (which is the popup itself when the user
				     * is pointing inside it). Convert the screen coords and forward
				     * a logical DIP delta: one notch = WHEEL_DELTA (=120) physical
				     * units, we translate that to 3 lines ~= 48 DIP per notch.       */
					if (popup && popup->mouse_cb) {
						int   wheel = GET_WHEEL_DELTA_WPARAM(wp); /* +ve = up */
						UINT  dpi   = GetDpiForWindow(hwnd);
						float scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;

						POINT pt    = {( short ) LOWORD(lp), ( short ) HIWORD(lp)};
						ScreenToClient(hwnd, &pt);
						float x  = ( float ) pt.x / scale;

						/* 48 DIP per 120-unit notch (~3 line items). */
						float dy = (( float ) wheel / ( float ) WHEEL_DELTA) * 48.0f;

						popup->mouse_cb(popup->mouse_ctx, popup, FLUX_POPUP_MOUSE_WHEEL, x, dy);
					}
				return 0;
			}

			case WM_TIMER : {
					if (popup && wp == POPUP_ANIM_TIMER_ID) {
							if (popup->anim_active) {
								float elapsed_ms = popup_elapsed_ms_qpc(&popup->anim_start_qpc);
								bool  finished   = (elapsed_ms >= popup->anim_duration_ms);
								float t          = finished ? 1.0f : (elapsed_ms / popup->anim_duration_ms);
								float ey         = popup_ease_cp3x0(t);

									switch (popup->anim_style) {
									case FLUX_POPUP_ANIM_FLYOUT : popup_apply_flyout_frame(popup, ey); break;
									case FLUX_POPUP_ANIM_TOOLTIP :
										/* Fade is linear per WinUI FadeInThemeAnimation. */
										popup_apply_tooltip_frame(popup, t);
										break;
									case FLUX_POPUP_ANIM_MENU :
										default               : {
											float sy = POPUP_ANIM_SCALE_FROM + (1.0f - POPUP_ANIM_SCALE_FROM) * ey;
											popup_apply_animated_rect(popup, sy);
											break;
										}
									}

									if (finished) {
										popup->anim_active = false;
										KillTimer(hwnd, popup->anim_timer_id);
										popup->anim_timer_id = 0;
									}
								InvalidateRect(hwnd, NULL, FALSE);
							}
							else {
								KillTimer(hwnd, popup->anim_timer_id);
								popup->anim_timer_id = 0;
							}
					}
				return 0;
			}

			case WM_PAINT : {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
					if (popup && popup->graphics) {
						flux_graphics_begin_draw(popup->graphics);
						FluxColor bg = flux_color_rgba(0, 0, 0, 0);
						flux_graphics_clear(popup->graphics, bg);

						/* ── Animation rendering ───────────────────────────────
						 * The HWND itself is resized to the current animated
						 * (ScaleY × final_ph) in the WM_TIMER handler, so DWM's
						 * system-backdrop and rounded corners already track the
						 * animation. The D2D content is drawn at its FINAL layout
						 * size, and anything outside the current HWND bounds is
						 * clipped naturally by the render target.
						 *
						 * For BOTTOM placement (top-pinned) this requires no
						 * transform: content top at y=0 of RT = HWND top = final
						 * top, and the bottom half of the menu is clipped by the
						 * shrunken HWND bottom.
						 *
						 * For TOP placement (bottom-pinned) we translate content
						 * up by (current_h - final_h) DIPs so its bottom aligns
						 * with the current HWND bottom, and the top portion is
						 * clipped by the HWND top edge.                             */
						ID2D1RenderTarget *rt = ( ID2D1RenderTarget * ) flux_graphics_get_d2d_context(popup->graphics);

						float              translate_y = 0.0f;
							if (popup->anim_active
								&& popup->anim_style == FLUX_POPUP_ANIM_MENU
								&& popup->final_placement == FLUX_PLACEMENT_TOP)
							{
								UINT  dpi = GetDpiForWindow(hwnd);
								float sc  = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;
								RECT  wr;
								GetClientRect(hwnd, &wr);
								float cur_h_dip = (( float ) (wr.bottom - wr.top)) / sc;
								float fin_h_dip = (( float ) popup->final_ph) / sc;
								translate_y     = cur_h_dip - fin_h_dip; /* ≤ 0 */
							}

							if (rt && fabsf(translate_y) > 0.1f) {
								D2D1_MATRIX_3X2_F xform;
								xform._11 = 1.0f;
								xform._12 = 0.0f;
								xform._21 = 0.0f;
								xform._22 = 1.0f;
								xform._31 = 0.0f;
								xform._32 = translate_y;
								ID2D1RenderTarget_SetTransform(rt, &xform);
							}

						/* Call paint callback if set (tooltip / flyout / menu rendering) */
						if (popup->paint_cb) popup->paint_cb(popup->paint_ctx, popup);

							if (rt && fabsf(translate_y) > 0.1f) {
								D2D1_MATRIX_3X2_F identity;
								identity._11 = 1.0f;
								identity._12 = 0.0f;
								identity._21 = 0.0f;
								identity._22 = 1.0f;
								identity._31 = 0.0f;
								identity._32 = 0.0f;
								ID2D1RenderTarget_SetTransform(rt, &identity);
							}

						flux_graphics_end_draw(popup->graphics);
						flux_graphics_present(popup->graphics, true);
						flux_graphics_commit(popup->graphics);
					}
				EndPaint(hwnd, &ps);
				return 0;
			}

		case WM_ACTIVATE :
			if (popup && popup->dismiss_on_outside && LOWORD(wp) == WA_INACTIVE) flux_popup_dismiss(popup);
			return 0;

		case WM_MOUSEACTIVATE :
			/* Prevent the popup from stealing activation from owner */
			return MA_NOACTIVATE;

		case WM_SIZE :
				if (popup && popup->graphics) {
					int w = LOWORD(lp);
					int h = HIWORD(lp);
					if (w > 0 && h > 0) flux_graphics_resize(popup->graphics, w, h);
				}
			return 0;

		default : break;
		}

	return DefWindowProcW(hwnd, msg, wp, lp);
}
