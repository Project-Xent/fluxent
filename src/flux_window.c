#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"

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
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#define DWMSBT_NONE              0
#define DWMSBT_MAINWINDOW        2
#define DWMSBT_TRANSIENTWINDOW   3
#define DWMSBT_TABBEDWINDOW      4

static const wchar_t FLUX_WND_CLASS[] = L"FluxentWindowClass";
static bool class_registered = false;

struct FluxWindow {
    HWND hwnd;
    FluxDpiInfo dpi;
    FluxWindowConfig config;
    FluxGraphics *gfx;
    bool render_requested;
    bool com_initialized;
    int cursor_type;
    HCURSOR cursor_arrow;
    HCURSOR cursor_ibeam;
    HCURSOR cursor_hand;

    FluxRenderCallback on_render;
    void *render_ctx;
    FluxResizeCallback on_resize;
    void *resize_ctx;
    FluxDpiChangedCallback on_dpi_changed;
    void *dpi_changed_ctx;
    FluxMouseCallback on_mouse;
    void *mouse_ctx;
    FluxKeyCallback on_key;
    void *key_ctx;
    FluxCharCallback on_char;
    void *char_ctx;
    FluxImeCompositionCallback on_ime_composition;
    void *ime_composition_ctx;
    FluxContextMenuCallback on_context_menu;
    void *context_menu_ctx;
    FluxScrollCallback on_scroll;
    void *scroll_ctx;
    FluxSettingChangedCallback on_setting_changed;
    void *setting_changed_ctx;
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
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        win = (FluxWindow *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
        win->hwnd = hwnd;
    } else {
        win = (FluxWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (!win)
        return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        win->cursor_arrow = LoadCursorW(NULL, IDC_ARROW);
        win->cursor_ibeam = LoadCursorW(NULL, IDC_IBEAM);
        win->cursor_hand  = LoadCursorW(NULL, IDC_HAND);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            switch (win->cursor_type) {
            case 1:  SetCursor(win->cursor_ibeam); break;
            case 2:  SetCursor(win->cursor_hand);  break;
            default: SetCursor(win->cursor_arrow);  break;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);

        if (w > 0 && h > 0) {
            if (win->gfx)
                flux_graphics_resize(win->gfx, w, h);
            if (win->on_resize)
                win->on_resize(win->resize_ctx, w, h);
            /* Render immediately so layout updates during modal resize drag */
            if (win->on_render)
                win->on_render(win->render_ctx);
            win->render_requested = false;
        }
        return 0;
    }

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wp);
        win->dpi.dpi_x = (float)dpi;
        win->dpi.dpi_y = (float)dpi;
        if (win->gfx)
            flux_graphics_set_dpi(win->gfx, win->dpi);
        RECT *suggested = (RECT *)lp;
        if (suggested) {
            SetWindowPos(hwnd, NULL,
                         suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (win->on_dpi_changed)
            win->on_dpi_changed(win->dpi_changed_ctx, win->dpi);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        win->render_requested = true;
        return 0;
    }

    case WM_MOUSEMOVE: {
        float scale = win->dpi.dpi_x / 96.0f;
        float x = (float)GET_X_LPARAM(lp) / scale;
        float y = (float)GET_Y_LPARAM(lp) / scale;
        if (win->on_mouse)
            win->on_mouse(win->mouse_ctx, x, y, -1, false);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float scale = win->dpi.dpi_x / 96.0f;
        float x = (float)GET_X_LPARAM(lp) / scale;
        float y = (float)GET_Y_LPARAM(lp) / scale;
        SetCapture(hwnd);
        if (win->on_mouse)
            win->on_mouse(win->mouse_ctx, x, y, 0, true);
        return 0;
    }

    case WM_LBUTTONUP: {
        float scale = win->dpi.dpi_x / 96.0f;
        float x = (float)GET_X_LPARAM(lp) / scale;
        float y = (float)GET_Y_LPARAM(lp) / scale;
        ReleaseCapture();
        if (win->on_mouse)
            win->on_mouse(win->mouse_ctx, x, y, 0, false);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        float scale = win->dpi.dpi_x / 96.0f;
        float x = (float)GET_X_LPARAM(lp) / scale;
        float y = (float)GET_Y_LPARAM(lp) / scale;
        if (win->on_mouse)
            win->on_mouse(win->mouse_ctx, x, y, 1, true);
        return 0;
    }

    case WM_RBUTTONUP: {
        float scale = win->dpi.dpi_x / 96.0f;
        float x = (float)GET_X_LPARAM(lp) / scale;
        float y = (float)GET_Y_LPARAM(lp) / scale;
        if (win->on_mouse)
            win->on_mouse(win->mouse_ctx, x, y, 1, false);
        if (win->on_context_menu)
            win->on_context_menu(win->context_menu_ctx, x, y);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (win->on_key)
            win->on_key(win->key_ctx, (unsigned int)wp, true);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (win->on_key)
            win->on_key(win->key_ctx, (unsigned int)wp, false);
        return 0;

    case WM_CHAR:
        if (win->on_char)
            win->on_char(win->char_ctx, (wchar_t)wp);
        return 0;

    case WM_IME_STARTCOMPOSITION:
        return 0;

    case WM_IME_COMPOSITION: {
        if (win->on_ime_composition && (lp & GCS_COMPSTR)) {
            HIMC hImc = ImmGetContext(hwnd);
            if (hImc) {
                LONG comp_len = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
                if (comp_len > 0) {
                    wchar_t *comp_buf = (wchar_t *)_alloca(comp_len + sizeof(wchar_t));
                    ImmGetCompositionStringW(hImc, GCS_COMPSTR, comp_buf, comp_len);
                    comp_buf[comp_len / sizeof(wchar_t)] = 0;
                    int cursor_pos = (int)ImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
                    win->on_ime_composition(win->ime_composition_ctx, comp_buf, cursor_pos);
                } else {
                    win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
                }
                ImmReleaseContext(hwnd, hImc);
            }
        }
        /* Fall through to DefWindowProc so the system still generates WM_CHAR for committed text */
        break;
    }

    case WM_IME_ENDCOMPOSITION:
        if (win->on_ime_composition)
            win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
        break;

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (win->on_setting_changed)
            win->on_setting_changed(win->setting_changed_ctx);
        win->render_requested = true;
        return 0;

    case WM_DISPLAYCHANGE:
        if (win->gfx && !flux_graphics_is_device_current(win->gfx)) {
            flux_graphics_handle_device_change(win->gfx);
            win->render_requested = true;
        }
        return 0;

    case WM_MOUSEWHEEL: {
        float scale = win->dpi.dpi_x / 96.0f;
        POINT pt;
        pt.x = GET_X_LPARAM(lp);
        pt.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &pt);
        float x = (float)pt.x / scale;
        float y = (float)pt.y / scale;
        float delta = (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.0f;
        if (win->on_scroll)
            win->on_scroll(win->scroll_ctx, x, y, delta);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

HRESULT flux_window_create(const FluxWindowConfig *cfg, FluxWindow **out) {
    if (!out) return E_INVALIDARG;
    *out = NULL;

    FluxWindow *win = (FluxWindow *)calloc(1, sizeof(*win));
    if (!win) return E_OUTOFMEMORY;

    if (cfg)
        win->config = *cfg;
    if (!win->config.title)  win->config.title = L"Fluxent";
    if (win->config.width <= 0)  win->config.width = 800;
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
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = flux_wnd_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = FLUX_WND_CLASS;

        if (!RegisterClassExW(&wc)) {
            free(win);
            return HRESULT_FROM_WIN32(GetLastError());
        }
        class_registered = true;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;

    if (!win->config.resizable)
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    UINT sys_dpi = GetDpiForSystem();
    float scale = (float)sys_dpi / 96.0f;

    int cw = (int)(win->config.width * scale);
    int ch = (int)(win->config.height * scale);

    RECT rect = {0, 0, cw, ch};
    AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, sys_dpi);

    int ww = rect.right - rect.left;
    int wh = rect.bottom - rect.top;

    win->hwnd = CreateWindowExW(ex_style, FLUX_WND_CLASS, win->config.title, style,
                                CW_USEDEFAULT, CW_USEDEFAULT, ww, wh,
                                NULL, NULL, hinst, win);
    if (!win->hwnd) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        free(win);
        return hr;
    }

    UINT dpi = GetDpiForWindow(win->hwnd);
    win->dpi.dpi_x = (float)dpi;
    win->dpi.dpi_y = (float)dpi;

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

HWND flux_window_hwnd(const FluxWindow *win) {
    return win ? win->hwnd : NULL;
}

FluxSize flux_window_client_size(const FluxWindow *win) {
    if (!win || !win->hwnd) return (FluxSize){0, 0};
    RECT rc;
    GetClientRect(win->hwnd, &rc);
    return (FluxSize){(float)(rc.right - rc.left), (float)(rc.bottom - rc.top)};
}

FluxDpiInfo flux_window_dpi(const FluxWindow *win) {
    if (!win) return (FluxDpiInfo){96.0f, 96.0f};
    return win->dpi;
}

void flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx) {
    if (!win) return;
    win->on_render = cb;
    win->render_ctx = ctx;
}

void flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx) {
    if (!win) return;
    win->on_resize = cb;
    win->resize_ctx = ctx;
}

void flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx) {
    if (!win) return;
    win->on_dpi_changed = cb;
    win->dpi_changed_ctx = ctx;
}

void flux_window_set_mouse_callback(FluxWindow *win, FluxMouseCallback cb, void *ctx) {
    if (!win) return;
    win->on_mouse = cb;
    win->mouse_ctx = ctx;
}

void flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx) {
    if (!win) return;
    win->on_key = cb;
    win->key_ctx = ctx;
}

void flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx) {
    if (!win) return;
    win->on_char = cb;
    win->char_ctx = ctx;
}

void flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx) {
    if (!win) return;
    win->on_setting_changed = cb;
    win->setting_changed_ctx = ctx;
}

void flux_window_set_ime_composition_callback(FluxWindow *win, FluxImeCompositionCallback cb, void *ctx) {
    if (!win) return;
    win->on_ime_composition = cb;
    win->ime_composition_ctx = ctx;
}

void flux_window_set_context_menu_callback(FluxWindow *win, FluxContextMenuCallback cb, void *ctx) {
    if (!win) return;
    win->on_context_menu = cb;
    win->context_menu_ctx = ctx;
}

void flux_window_set_scroll_callback(FluxWindow *win, FluxScrollCallback cb, void *ctx) {
    if (!win) return;
    win->on_scroll = cb;
    win->scroll_ctx = ctx;
}

void flux_window_set_backdrop(FluxWindow *win, int backdrop_type) {
    if (!win || !win->hwnd) return;
    DWORD bd = DWMSBT_NONE;
    switch (backdrop_type) {
    case 1: bd = DWMSBT_MAINWINDOW;       break;
    case 2: bd = DWMSBT_TABBEDWINDOW;     break;
    case 3: bd = DWMSBT_TRANSIENTWINDOW;  break;
    default: bd = DWMSBT_NONE;            break;
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

void flux_window_set_title(FluxWindow *win, const wchar_t *title) {
    if (!win || !win->hwnd || !title) return;
    SetWindowTextW(win->hwnd, title);
}

void flux_window_set_ime_position(FluxWindow *win, int x, int y, int height) {
    if (!win || !win->hwnd) return;
    HIMC hImc = ImmGetContext(win->hwnd);
    if (!hImc) return;

    COMPOSITIONFORM cf;
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = x;
    cf.ptCurrentPos.y = y;
    ImmSetCompositionWindow(hImc, &cf);

    CANDIDATEFORM cand;
    cand.dwIndex = 0;
    cand.dwStyle = CFS_CANDIDATEPOS;
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
        case 1:  c = win->cursor_ibeam; break;
        case 2:  c = win->cursor_hand;  break;
        default: c = win->cursor_arrow;  break;
        }
        SetCursor(c);
    }
}

void flux_window_request_render(FluxWindow *win) {
    if (win) win->render_requested = true;
}

int flux_window_run(FluxWindow *win) {
    if (!win) return -1;

    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (win->render_requested) {
            win->render_requested = false;

            if (win->gfx && !flux_graphics_is_device_current(win->gfx))
                flux_graphics_handle_device_change(win->gfx);

            if (win->on_render)
                win->on_render(win->render_ctx);
        }

        if (!win->render_requested)
            MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

FluxGraphics *flux_window_get_graphics(FluxWindow *win) {
    return win ? win->gfx : NULL;
}

void flux_window_close(FluxWindow *win) {
    if (win && win->hwnd)
        PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
}
