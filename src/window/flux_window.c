#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_popup.h"

#include <stdlib.h>
#include <string.h>
#include <objbase.h>
#include <imm.h>
#ifdef _MSC_VER
  #pragma comment(lib, "imm32.lib")
#endif

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <dwmapi.h>
#include <windowsx.h>

/* Windows touch gesture + feedback APIs. Both live in user32.lib which
 * is implicitly linked. `GESTURECONFIG` / `SetGestureConfig` require
 * `WINVER >= 0x0601`; `SetWindowFeedbackSetting` requires `WINVER >= 0x0602`. */
#ifndef WINVER
  #define WINVER 0x0602
#endif
#include <winuser.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
  #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
  #define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
  #define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif
#ifndef WS_EX_NOREDIRECTIONBITMAP
  #define WS_EX_NOREDIRECTIONBITMAP 0x00200000l
#endif

#define DWMSBT_NONE            0
#define DWMSBT_MAINWINDOW      2
#define DWMSBT_TRANSIENTWINDOW 3
#define DWMSBT_TABBEDWINDOW    4

static wchar_t const FLUX_WND_CLASS [] = L"FluxentWindowClass";
static bool          class_registered  = false;

struct FluxWindow {
	HWND                       hwnd;
	FluxDpiInfo                dpi;
	FluxWindowConfig           config;
	FluxGraphics              *gfx;
	bool                       render_requested;
	bool                       com_initialized;
	int                        cursor_type;
	HCURSOR                    cursor_arrow;
	HCURSOR                    cursor_ibeam;
	HCURSOR                    cursor_hand;

	/* Primary touch pid — set on the first finger-down while idle, held
	 * until its matching UP.  Additional concurrent touch pids are
	 * dropped so a second finger / palm can't hijack the gesture.  0
	 * when no touch stream is active. */
	uint32_t                   primary_touch_pid;

	/* Last known button bitmask for the active mouse pointer.  Used to
	 * derive per-button DOWN/UP edges from WM_POINTERUPDATE, since
	 * EnableMouseInPointer only fires WM_POINTERDOWN/UP once per
	 * contact span — not per button. */
	uint32_t                   mouse_buttons;

	FluxRenderCallback         on_render;
	void                      *render_ctx;
	FluxResizeCallback         on_resize;
	void                      *resize_ctx;
	FluxDpiChangedCallback     on_dpi_changed;
	void                      *dpi_changed_ctx;
	FluxPointerCallback        on_pointer;
	void                      *pointer_ctx;
	FluxKeyCallback            on_key;
	void                      *key_ctx;
	FluxCharCallback           on_char;
	void                      *char_ctx;
	FluxImeCompositionCallback on_ime_composition;
	void                      *ime_composition_ctx;
	FluxSettingChangedCallback on_setting_changed;
	void                      *setting_changed_ctx;
};

static void setup_dwm_backdrop(FluxWindow *win) {
	BOOL dark = win->config.dark_mode ? TRUE : FALSE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	BOOL host = TRUE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &host, sizeof(host));

	flux_window_set_backdrop(win, win->config.backdrop);
}

static LRESULT CALLBACK flux_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	FluxWindow *win = NULL;

		if (msg == WM_NCCREATE) {
			CREATESTRUCTW *cs = ( CREATESTRUCTW * ) lp;
			win               = ( FluxWindow * ) cs->lpCreateParams;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, ( LONG_PTR ) win);
			win->hwnd = hwnd;
		}
		else { win = ( FluxWindow * ) GetWindowLongPtrW(hwnd, GWLP_USERDATA); }

	if (!win) return DefWindowProcW(hwnd, msg, wp, lp);

		switch (msg) {
		case WM_CREATE :
			win->cursor_arrow = LoadCursorW(NULL, IDC_ARROW);
			win->cursor_ibeam = LoadCursorW(NULL, IDC_IBEAM);
			win->cursor_hand  = LoadCursorW(NULL, IDC_HAND);

			/* ── Pointer Input Stack (Windows 8+) ───────────────────────
			 * Route all pointer input (mouse/touch/pen/touchpad) through
			 * the WM_POINTER* message family.  This gives us:
			 *   • Reliable PT_TOUCH / PT_PEN / PT_MOUSE classification
			 *     via GetPointerType() — replaces the unreliable
			 *     GetMessageExtraInfo() 0x80-bit heuristic.
			 *   • Multi-touch contact IDs for DirectManipulation handoff.
			 *   • Pen pressure / tilt (via GetPointerPenInfo if we want).
			 *
			 * Once EnableMouseInPointer(TRUE) succeeds, mouse input comes
			 * as WM_POINTER* only (WM_MOUSE* is suppressed for mouse).
			 * Touch/pen always generate WM_POINTER* on Win8+. */
			{
				typedef BOOL(WINAPI * PFN_EnableMouseInPointer)(BOOL);
				HMODULE user32 = GetModuleHandleW(L"user32.dll");
					if (user32) {
						PFN_EnableMouseInPointer p_emip
						  = ( PFN_EnableMouseInPointer ) ( void * ) GetProcAddress(user32, "EnableMouseInPointer");
						if (p_emip) p_emip(TRUE);
					}
			}

			/* Block the system gesture recognizer so single-finger drags
			 * reach us as WM_POINTERUPDATE instead of being consumed into
			 * WM_GESTURE pan messages.  We KEEP press-and-hold → right-click
			 * enabled — that one fires as WM_CONTEXTMENU, not WM_GESTURE,
			 * and is the standard Windows touch right-click affordance. */
			{
				GESTURECONFIG gc = {0, 0, GC_ALLGESTURES};
				SetGestureConfig(hwnd, 0, 1, &gc, sizeof(GESTURECONFIG));
			}
			return 0;

		case WM_SETCURSOR :
				if (LOWORD(lp) == HTCLIENT) {
						switch (win->cursor_type) {
						case 1  : SetCursor(win->cursor_ibeam); break;
						case 2  : SetCursor(win->cursor_hand); break;
						default : SetCursor(win->cursor_arrow); break;
						}
					return TRUE;
				}
			break;

		case WM_DESTROY : PostQuitMessage(0); return 0;

		case WM_CLOSE   : DestroyWindow(hwnd); return 0;

		/* ── Transient-popup interruption ────────────────────────
		 * Close any open flyout/menu/tooltip when the owner window loses
		 * activation (Alt-Tab, click elsewhere) or is moved by the user.
		 * This matches WinUI's LightDismissOverlayMode behaviour.          */
		case WM_ACTIVATE :
			if (LOWORD(wp) == WA_INACTIVE) flux_popup_dismiss_all_for_owner(hwnd);
			break;

		case WM_MOVE :
		case WM_ENTERSIZEMOVE : flux_popup_dismiss_all_for_owner(hwnd); break;

			case WM_SIZE      : {
				int w = LOWORD(lp);
				int h = HIWORD(lp);

					if (w > 0 && h > 0) {
						if (win->gfx) flux_graphics_resize(win->gfx, w, h);
						if (win->on_resize) win->on_resize(win->resize_ctx, w, h);
						/* Render immediately so layout updates during modal resize drag */
						if (win->on_render) win->on_render(win->render_ctx);
						win->render_requested = false;
					}
				return 0;
			}

			case WM_DPICHANGED : {
				UINT dpi       = HIWORD(wp);
				win->dpi.dpi_x = ( float ) dpi;
				win->dpi.dpi_y = ( float ) dpi;
				if (win->gfx) flux_graphics_set_dpi(win->gfx, win->dpi);
				RECT *suggested = ( RECT * ) lp;
					if (suggested) {
						SetWindowPos(
						  hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
						  suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE
						);
					}
				if (win->on_dpi_changed) win->on_dpi_changed(win->dpi_changed_ctx, win->dpi);
				return 0;
			}

		case WM_ERASEBKGND : return 1;

			case WM_PAINT  : {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				win->render_requested = true;
				return 0;
			}

			/* ═══════════════════════════════════════════════════════════════════
			 *  Input path — Win10+
			 *
			 *  EnableMouseInPointer(TRUE) is enabled in WM_CREATE so mouse, touch
			 *  and pen all deliver input through the WM_POINTER* family.  The old
			 *  WM_MOUSE* cases are gone; we only keep WM_MOUSEWHEEL/WM_MOUSEHWHEEL
			 *  because touchpad smooth-scroll still comes that way on most drivers.
			 *
			 *  This block is PURE translation: Win32 message → FluxPointerEvent →
			 *  on_pointer().  It does not latch any per-control state, it does not
			 *  dismiss popups, and it does not decide what "pressed" means.
			 * ═══════════════════════════════════════════════════════════════════ */

#ifndef PT_TOUCHPAD
  #define PT_TOUCHPAD 5
#endif

		case WM_POINTERDOWN :
		case WM_POINTERUPDATE :
			case WM_POINTERUP : {
				UINT32             pid   = GET_POINTERID_WPARAM(wp);
				POINTER_INPUT_TYPE ptype = PT_POINTER;
				GetPointerType(pid, &ptype);
				if (ptype == PT_TOUCHPAD) return 0; /* not a real contact */

				bool is_touch = (ptype == PT_TOUCH);
				bool is_pen   = (ptype == PT_PEN);
				bool is_mouse = (ptype == PT_MOUSE);

					/* Touch: latch the first active finger; drop the rest. */
					if (is_touch) {
							if (msg == WM_POINTERDOWN) {
								if (win->primary_touch_pid != 0 && win->primary_touch_pid != pid) return 0;
								win->primary_touch_pid = pid;
							}
							else if (win->primary_touch_pid != pid) { return 0; }
					}

				POINTER_INFO pi = {0};
				if (!GetPointerInfo(pid, &pi)) return 0;
				UINT32 pflags = pi.pointerFlags;

				/* Drop stale hover pulses — pen hover events come as UPDATE with
				 * no INCONTACT flag; we currently don't surface hover for touch/
				 * pen so skip them entirely.  Mouse is always "in contact" in
				 * the Windows sense. */
				if (!is_mouse && !(pflags & POINTER_FLAG_INCONTACT) && msg == WM_POINTERUPDATE) return 0;

				POINT scr = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				ScreenToClient(hwnd, &scr);
				float            scale = win->dpi.dpi_x / 96.0f;
				float            x     = ( float ) scr.x / scale;
				float            y     = ( float ) scr.y / scale;

				FluxPointerEvent ev    = {0};
				ev.device              = is_touch ? FLUX_POINTER_TOUCH : is_pen ? FLUX_POINTER_PEN : FLUX_POINTER_MOUSE;
				ev.pointer_id          = pid;
				ev.x                   = x;
				ev.y                   = y;

				/* Decode held buttons from pointer flags. */
				uint32_t new_buttons   = 0;
				if (pflags & POINTER_FLAG_FIRSTBUTTON) new_buttons |= FLUX_POINTER_BUTTON_LEFT;
				if (pflags & POINTER_FLAG_SECONDBUTTON) new_buttons |= FLUX_POINTER_BUTTON_RIGHT;
				if (pflags & POINTER_FLAG_THIRDBUTTON) new_buttons |= FLUX_POINTER_BUTTON_MIDDLE;

					if (is_mouse) {
						/* Mouse: derive button edges by diffing the bitmask against
						 * the previous one.  WM_POINTERDOWN / UP fire only on the
						 * first-contact / last-release transitions, so they're not
						 * reliable for secondary buttons — a diff is the only
						 * trustworthy signal. */
						uint32_t old_buttons = win->mouse_buttons;
						uint32_t pressed     = new_buttons & ~old_buttons;
						uint32_t released    = old_buttons & ~new_buttons;

							/* Emit DOWN for each newly-pressed button (LSB first). */
							for (uint32_t b = pressed; b; b &= b - 1) {
								uint32_t bit      = b & (0u - b);
								ev.kind           = FLUX_POINTER_DOWN;
								ev.buttons        = old_buttons | bit;
								ev.changed_button = bit;
								if (bit == FLUX_POINTER_BUTTON_LEFT) SetCapture(hwnd);
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
								old_buttons |= bit;
							}

							/* Emit UP for each newly-released button. */
							for (uint32_t b = released; b; b &= b - 1) {
								uint32_t bit      = b & (0u - b);
								ev.kind           = FLUX_POINTER_UP;
								ev.buttons        = old_buttons & ~bit;
								ev.changed_button = bit;
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
								old_buttons &= ~bit;

									/* Right-release is also a context-menu gesture. */
									if (bit == FLUX_POINTER_BUTTON_RIGHT) {
										ev.kind           = FLUX_POINTER_CONTEXT_MENU;
										ev.changed_button = 0;
										if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
									}
								if (bit == FLUX_POINTER_BUTTON_LEFT && old_buttons == 0) ReleaseCapture();
							}

						win->mouse_buttons = new_buttons;

							/* Plain MOVE (no button transition).  Emit one per UPDATE so
						     * hover tracking sees continuous motion. */
							if (msg == WM_POINTERUPDATE && pressed == 0 && released == 0) {
								ev.kind           = FLUX_POINTER_MOVE;
								ev.buttons        = new_buttons;
								ev.changed_button = 0;
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
							}
					}
					else {
							/* Touch / pen: WM_POINTERDOWN=contact, UP=lift, UPDATE=move.
						     * Only "left button" is meaningful. */
							if (msg == WM_POINTERDOWN) {
								SetCapture(hwnd);
								ev.kind           = FLUX_POINTER_DOWN;
								ev.buttons        = FLUX_POINTER_BUTTON_LEFT;
								ev.changed_button = FLUX_POINTER_BUTTON_LEFT;
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
							}
							else if (msg == WM_POINTERUP) {
								ev.kind           = FLUX_POINTER_UP;
								ev.buttons        = 0;
								ev.changed_button = FLUX_POINTER_BUTTON_LEFT;
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
								ReleaseCapture();
								if (is_touch && win->primary_touch_pid == pid) win->primary_touch_pid = 0;
							}
							else {
								ev.kind           = FLUX_POINTER_MOVE;
								ev.buttons        = (pflags & POINTER_FLAG_INCONTACT) ? FLUX_POINTER_BUTTON_LEFT : 0;
								ev.changed_button = 0;
								if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
							}
					}

				/* For touch, let DefWindowProc run so the system press-and-hold
				 * recognizer can fire WM_CONTEXTMENU.  For mouse/pen we're done. */
				if (is_touch) break;
				return 0;
			}

		case WM_POINTERWHEEL :
			case WM_POINTERHWHEEL : {
				UINT32             pid   = GET_POINTERID_WPARAM(wp);
				POINTER_INPUT_TYPE ptype = PT_POINTER;
				GetPointerType(pid, &ptype);
				if (ptype == PT_TOUCHPAD) return 0;

				POINT scr = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				ScreenToClient(hwnd, &scr);
				float            scale = win->dpi.dpi_x / 96.0f;
				float            delta = ( float ) GET_WHEEL_DELTA_WPARAM(wp) / ( float ) WHEEL_DELTA;

				FluxPointerEvent ev    = {0};
				ev.kind                = FLUX_POINTER_WHEEL;
				ev.device              = ptype == PT_TOUCH ? FLUX_POINTER_TOUCH
				                       : ptype == PT_PEN   ? FLUX_POINTER_PEN
				                                           : FLUX_POINTER_MOUSE;
				ev.pointer_id          = pid;
				ev.x                   = ( float ) scr.x / scale;
				ev.y                   = ( float ) scr.y / scale;
				if (msg == WM_POINTERHWHEEL) ev.wheel_dx = delta;
				else ev.wheel_dy = delta;
				if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
				return 0;
			}

		/* Capture lost mid-gesture (alt-tab, system grab).  Emit CANCEL so
		 * the router drops any latched pressed/touch-pan state cleanly. */
		case WM_POINTERCAPTURECHANGED :
			case WM_CAPTURECHANGED    : {
				FluxPointerEvent ev = {0};
				ev.kind             = FLUX_POINTER_CANCEL;
				ev.device           = FLUX_POINTER_MOUSE; /* best guess — policy layer doesn't care */
				POINT cur;
				GetCursorPos(&cur);
				ScreenToClient(hwnd, &cur);
				float scale            = win->dpi.dpi_x / 96.0f;
				ev.x                   = ( float ) cur.x / scale;
				ev.y                   = ( float ) cur.y / scale;
				win->primary_touch_pid = 0;
				win->mouse_buttons     = 0;
				if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
				return 0;
			}

			/* Touch long-press / keyboard Menu key / Shift+F10.  lParam -1,-1
			 * means "from keyboard" — we still emit at (0,0) and let the policy
			 * layer anchor to the current focus. */
			case WM_CONTEXTMENU : {
				int              sx = GET_X_LPARAM(lp), sy = GET_Y_LPARAM(lp);
				FluxPointerEvent ev = {0};
				ev.kind             = FLUX_POINTER_CONTEXT_MENU;
				ev.device           = FLUX_POINTER_TOUCH; /* press-and-hold is touch by convention */
					if (sx != -1 || sy != -1) {
						POINT scr = {sx, sy};
						ScreenToClient(hwnd, &scr);
						float scale = win->dpi.dpi_x / 96.0f;
						ev.x        = ( float ) scr.x / scale;
						ev.y        = ( float ) scr.y / scale;
					}
					else { ev.device = FLUX_POINTER_MOUSE; /* keyboard: no real coords */ }
				if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
				return 0;
			}

		case WM_KEYDOWN :
		case WM_SYSKEYDOWN :
			if (win->on_key) win->on_key(win->key_ctx, ( unsigned int ) wp, true);
			return 0;

		case WM_KEYUP :
		case WM_SYSKEYUP :
			if (win->on_key) win->on_key(win->key_ctx, ( unsigned int ) wp, false);
			return 0;

		case WM_CHAR :
			if (win->on_char) win->on_char(win->char_ctx, ( wchar_t ) wp);
			return 0;

		case WM_IME_STARTCOMPOSITION : return 0;

			case WM_IME_COMPOSITION  : {
					if (win->on_ime_composition && (lp & GCS_COMPSTR)) {
						HIMC hImc = ImmGetContext(hwnd);
							if (hImc) {
								LONG comp_len = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
									if (comp_len > 0) {
										wchar_t *comp_buf = ( wchar_t * ) _alloca(comp_len + sizeof(wchar_t));
										ImmGetCompositionStringW(hImc, GCS_COMPSTR, comp_buf, comp_len);
										comp_buf [comp_len / sizeof(wchar_t)] = 0;
										int cursor_pos = ( int ) ImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
										win->on_ime_composition(win->ime_composition_ctx, comp_buf, cursor_pos);
									}
									else { win->on_ime_composition(win->ime_composition_ctx, NULL, 0); }
								ImmReleaseContext(hwnd, hImc);
							}
					}
				/* Fall through to DefWindowProc so the system still generates WM_CHAR for committed text */
				break;
			}

		case WM_IME_ENDCOMPOSITION :
			if (win->on_ime_composition) win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
			break;

		case WM_SETTINGCHANGE :
		case WM_THEMECHANGED :
			if (win->on_setting_changed) win->on_setting_changed(win->setting_changed_ctx);
			win->render_requested = true;
			return 0;

		case WM_DISPLAYCHANGE :
				if (win->gfx && !flux_graphics_is_device_current(win->gfx)) {
					flux_graphics_handle_device_change(win->gfx);
					win->render_requested = true;
				}
			return 0;

		/* Touchpad smooth-scroll still comes through WM_MOUSE*WHEEL on most
		 * drivers — WM_POINTERWHEEL is only generated for discrete wheels.
		 * For mouse wheels we'll receive both; emit only one (the POINTER
		 * version wins, this case no-ops for PT_MOUSE via a quick test). */
		case WM_MOUSEWHEEL :
			case WM_MOUSEHWHEEL : {
				POINT pt;
				pt.x = GET_X_LPARAM(lp);
				pt.y = GET_Y_LPARAM(lp);
				ScreenToClient(hwnd, &pt);
				float            scale = win->dpi.dpi_x / 96.0f;
				float            delta = ( float ) GET_WHEEL_DELTA_WPARAM(wp) / ( float ) WHEEL_DELTA;

				FluxPointerEvent ev    = {0};
				ev.kind                = FLUX_POINTER_WHEEL;
				ev.device              = FLUX_POINTER_MOUSE;
				ev.x                   = ( float ) pt.x / scale;
				ev.y                   = ( float ) pt.y / scale;
				if (msg == WM_MOUSEHWHEEL) ev.wheel_dx = delta;
				else ev.wheel_dy = delta;
				if (win->on_pointer) win->on_pointer(win->pointer_ctx, &ev);
				return 0;
			}

		default : break;
		}

	return DefWindowProcW(hwnd, msg, wp, lp);
}

HRESULT flux_window_create(FluxWindowConfig const *cfg, FluxWindow **out) {
	if (!out) return E_INVALIDARG;
	*out            = NULL;

	FluxWindow *win = ( FluxWindow * ) calloc(1, sizeof(*win));
	if (!win) return E_OUTOFMEMORY;

	if (cfg) win->config = *cfg;
	if (!win->config.title) win->config.title = L"Fluxent";
	if (win->config.width <= 0) win->config.width = 800;
	if (win->config.height <= 0) win->config.height = 600;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
			free(win);
			return hr;
		}
	win->com_initialized = (hr == S_OK || hr == S_FALSE);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HINSTANCE hinst = GetModuleHandleW(NULL);

		if (!class_registered) {
			WNDCLASSEXW wc;
			memset(&wc, 0, sizeof(wc));
			wc.cbSize        = sizeof(wc);
			wc.style         = CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc   = flux_wnd_proc;
			wc.hInstance     = hinst;
			wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
			wc.lpszClassName = FLUX_WND_CLASS;

				if (!RegisterClassExW(&wc)) {
					free(win);
					return HRESULT_FROM_WIN32(GetLastError());
				}
			class_registered = true;
		}

	DWORD style    = WS_OVERLAPPEDWINDOW;
	DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;

	if (!win->config.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

	UINT  sys_dpi = GetDpiForSystem();
	float scale   = ( float ) sys_dpi / 96.0f;

	int   cw      = ( int ) (win->config.width * scale);
	int   ch      = ( int ) (win->config.height * scale);

	RECT  rect    = {0, 0, cw, ch};
	AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, sys_dpi);

	int ww    = rect.right - rect.left;
	int wh    = rect.bottom - rect.top;

	win->hwnd = CreateWindowExW(
	  ex_style, FLUX_WND_CLASS, win->config.title, style, CW_USEDEFAULT, CW_USEDEFAULT, ww, wh, NULL, NULL, hinst, win
	);
		if (!win->hwnd) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			free(win);
			return hr;
		}

	UINT dpi       = GetDpiForWindow(win->hwnd);
	win->dpi.dpi_x = ( float ) dpi;
	win->dpi.dpi_y = ( float ) dpi;

	setup_dwm_backdrop(win);

	win->gfx = flux_graphics_create();
		if (!win->gfx) {
			DestroyWindow(win->hwnd);
			free(win);
			return E_FAIL;
		}

	hr = flux_graphics_attach(win->gfx, win->hwnd);
		if (FAILED(hr)) {
			flux_graphics_destroy(win->gfx);
			DestroyWindow(win->hwnd);
			free(win);
			return hr;
		}

	flux_graphics_set_dpi(win->gfx, win->dpi);

	ShowWindow(win->hwnd, SW_SHOW);
	UpdateWindow(win->hwnd);

	*out = win;
	return S_OK;
}

void flux_window_destroy(FluxWindow *win) {
	if (!win) return;
	if (win->gfx) flux_graphics_destroy(win->gfx);
	if (win->hwnd) DestroyWindow(win->hwnd);
	if (win->com_initialized) CoUninitialize();
	free(win);
}

HWND     flux_window_hwnd(FluxWindow const *win) { return win ? win->hwnd : NULL; }

FluxSize flux_window_client_size(FluxWindow const *win) {
	if (!win || !win->hwnd) return (FluxSize) {0, 0};
	RECT rc;
	GetClientRect(win->hwnd, &rc);
	return (FluxSize) {( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

FluxDpiInfo flux_window_dpi(FluxWindow const *win) {
	if (!win) return (FluxDpiInfo) {96.0f, 96.0f};
	return win->dpi;
}

void flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx) {
	if (!win) return;
	win->on_render  = cb;
	win->render_ctx = ctx;
}

void flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx) {
	if (!win) return;
	win->on_resize  = cb;
	win->resize_ctx = ctx;
}

void flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx) {
	if (!win) return;
	win->on_dpi_changed  = cb;
	win->dpi_changed_ctx = ctx;
}

void flux_window_set_pointer_callback(FluxWindow *win, FluxPointerCallback cb, void *ctx) {
	if (!win) return;
	win->on_pointer  = cb;
	win->pointer_ctx = ctx;
}

void flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx) {
	if (!win) return;
	win->on_key  = cb;
	win->key_ctx = ctx;
}

void flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx) {
	if (!win) return;
	win->on_char  = cb;
	win->char_ctx = ctx;
}

void flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx) {
	if (!win) return;
	win->on_setting_changed  = cb;
	win->setting_changed_ctx = ctx;
}

void flux_window_set_ime_composition_callback(FluxWindow *win, FluxImeCompositionCallback cb, void *ctx) {
	if (!win) return;
	win->on_ime_composition  = cb;
	win->ime_composition_ctx = ctx;
}

void flux_window_set_backdrop(FluxWindow *win, int backdrop_type) {
	if (!win || !win->hwnd) return;
	DWORD bd = DWMSBT_NONE;
		switch (backdrop_type) {
		case 1  : bd = DWMSBT_MAINWINDOW; break;
		case 2  : bd = DWMSBT_TABBEDWINDOW; break;
		case 3  : bd = DWMSBT_TRANSIENTWINDOW; break;
		default : bd = DWMSBT_NONE; break;
		}
	DwmSetWindowAttribute(win->hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &bd, sizeof(bd));
	win->config.backdrop = backdrop_type;
}

void flux_window_set_dark_mode(FluxWindow *win, bool enabled) {
	if (!win || !win->hwnd) return;
	BOOL dark = enabled ? TRUE : FALSE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	win->config.dark_mode = enabled;
}

void flux_window_set_title(FluxWindow *win, wchar_t const *title) {
	if (!win || !win->hwnd || !title) return;
	SetWindowTextW(win->hwnd, title);
}

void flux_window_set_ime_position(FluxWindow *win, int x, int y, int height) {
	if (!win || !win->hwnd) return;
	HIMC hImc = ImmGetContext(win->hwnd);
	if (!hImc) return;

	COMPOSITIONFORM cf;
	cf.dwStyle        = CFS_POINT;
	cf.ptCurrentPos.x = x;
	cf.ptCurrentPos.y = y;
	ImmSetCompositionWindow(hImc, &cf);

	CANDIDATEFORM cand;
	cand.dwIndex        = 0;
	cand.dwStyle        = CFS_CANDIDATEPOS;
	cand.ptCurrentPos.x = x;
	cand.ptCurrentPos.y = y + height;
	ImmSetCandidateWindow(hImc, &cand);

	ImmReleaseContext(win->hwnd, hImc);
}

void flux_window_set_cursor(FluxWindow *win, int cursor_type) {
	if (!win) return;
	if (win->cursor_type == cursor_type) return;
	win->cursor_type = cursor_type;
		/* Force an immediate visual update */
		if (win->hwnd) {
			HCURSOR c;
				switch (cursor_type) {
				case 1  : c = win->cursor_ibeam; break;
				case 2  : c = win->cursor_hand; break;
				default : c = win->cursor_arrow; break;
				}
			SetCursor(c);
		}
}

void flux_window_request_render(FluxWindow *win) {
	if (win) win->render_requested = true;
}

void flux_window_abandon_pointer(FluxWindow *win, uint32_t pointer_id) {
	if (!win) return;
	/* Match by pid to avoid dropping an unrelated touch that raced in. */
	if (win->primary_touch_pid == pointer_id) win->primary_touch_pid = 0;
	if (win->hwnd && GetCapture() == win->hwnd) ReleaseCapture();
}

int flux_window_run(FluxWindow *win) {
	if (!win) return -1;

		for (;;) {
			MSG msg;
				while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) return ( int ) msg.wParam;
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}

				if (win->render_requested) {
					win->render_requested = false;

					if (win->gfx && !flux_graphics_is_device_current(win->gfx))
						flux_graphics_handle_device_change(win->gfx);

					if (win->on_render) win->on_render(win->render_ctx);
				}

			if (!win->render_requested)
				MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}
}

FluxGraphics *flux_window_get_graphics(FluxWindow *win) { return win ? win->gfx : NULL; }

void          flux_window_close(FluxWindow *win) {
	if (win && win->hwnd) PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
}
