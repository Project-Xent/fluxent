#include "fluxent/flux_popup.h"

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <stdlib.h>
#include <string.h>

/* ------- Popup struct ------- */

struct FluxPopup {
    FluxWindow   *owner;
    HWND          owner_hwnd;
    HWND          popup_hwnd;
    FluxGraphics *graphics;

    XentContext   *ctx;
    FluxNodeStore *store;
    XentNodeId     content_root;

    FluxRect       anchor;        /* screen coords */
    FluxPlacement  placement;
    float          content_w;
    float          content_h;

    bool           is_visible;
    bool           dismiss_on_outside;

    FluxPopupDismissCallback dismiss_cb;
    void                    *dismiss_ctx;
};

/* ------- Forward declarations ------- */
static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void popup_position(FluxPopup *popup);
static const wchar_t *POPUP_CLASS_NAME = L"FluxPopupClass";
static ATOM s_popup_class = 0;

/* ------- Register window class (once) ------- */
static void ensure_popup_class(void) {
    if (s_popup_class) return;
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = popup_wnd_proc;
    wc.hInstance      = GetModuleHandleW(NULL);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = POPUP_CLASS_NAME;
    s_popup_class = RegisterClassExW(&wc);
}

/* ------- Create / Destroy ------- */

FluxPopup *flux_popup_create(FluxWindow *owner) {
    if (!owner) return NULL;

    ensure_popup_class();

    FluxPopup *popup = (FluxPopup *)calloc(1, sizeof(*popup));
    if (!popup) return NULL;

    popup->owner      = owner;
    popup->owner_hwnd = flux_window_hwnd(owner);
    popup->dismiss_on_outside = true;
    popup->content_w  = 200.0f;
    popup->content_h  = 100.0f;
    popup->placement  = FLUX_PLACEMENT_BOTTOM;
    popup->content_root = XENT_NODE_INVALID;

    /* Create the popup HWND (initially hidden) */
    popup->popup_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        POPUP_CLASS_NAME,
        L"",
        WS_POPUP | WS_CLIPSIBLINGS,
        0, 0, 1, 1,
        popup->owner_hwnd,
        NULL,
        GetModuleHandleW(NULL),
        popup);

    if (!popup->popup_hwnd) {
        free(popup);
        return NULL;
    }

    SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, (LONG_PTR)popup);

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

    if (popup->popup_hwnd) {
        SetWindowLongPtrW(popup->popup_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(popup->popup_hwnd);
    }
    if (popup->graphics)
        flux_graphics_destroy(popup->graphics);

    free(popup);
}

/* ------- Content ------- */

void flux_popup_set_content(FluxPopup *popup, XentContext *ctx,
                            FluxNodeStore *store, XentNodeId content_root) {
    if (!popup) return;
    popup->ctx          = ctx;
    popup->store        = store;
    popup->content_root = content_root;
}

void flux_popup_set_size(FluxPopup *popup, float width, float height) {
    if (!popup) return;
    popup->content_w = width;
    popup->content_h = height;
    if (popup->is_visible)
        popup_position(popup);
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

/* ------- Position calculation with flip logic ------- */

static void popup_position(FluxPopup *popup) {
    if (!popup || !popup->popup_hwnd) return;

    float aw = popup->content_w;
    float ah = popup->content_h;

    /* Get DPI scale */
    UINT dpi = GetDpiForWindow(popup->owner_hwnd);
    float scale = dpi / 96.0f;
    int pw = (int)(aw * scale + 0.5f);
    int ph = (int)(ah * scale + 0.5f);

    /* Get monitor work area for flip logic */
    HMONITOR mon = MonitorFromWindow(popup->owner_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { .cbSize = sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    RECT work = mi.rcWork;

    FluxRect a = popup->anchor;
    int x = 0, y = 0;
    FluxPlacement pl = popup->placement;

    /* Try placement, flip if it goes off screen */
    for (int attempt = 0; attempt < 2; attempt++) {
        switch (pl) {
        case FLUX_PLACEMENT_BOTTOM:
            x = (int)(a.x + (a.w - aw * scale) * 0.5f);
            y = (int)(a.y + a.h + 4 * scale);
            if (y + ph > work.bottom && attempt == 0) { pl = FLUX_PLACEMENT_TOP; continue; }
            break;
        case FLUX_PLACEMENT_TOP:
            x = (int)(a.x + (a.w - aw * scale) * 0.5f);
            y = (int)(a.y - ph - 4 * scale);
            if (y < work.top && attempt == 0) { pl = FLUX_PLACEMENT_BOTTOM; continue; }
            break;
        case FLUX_PLACEMENT_RIGHT:
            x = (int)(a.x + a.w + 4 * scale);
            y = (int)(a.y + (a.h - ah * scale) * 0.5f);
            if (x + pw > work.right && attempt == 0) { pl = FLUX_PLACEMENT_LEFT; continue; }
            break;
        case FLUX_PLACEMENT_LEFT:
            x = (int)(a.x - pw - 4 * scale);
            y = (int)(a.y + (a.h - ah * scale) * 0.5f);
            if (x < work.left && attempt == 0) { pl = FLUX_PLACEMENT_RIGHT; continue; }
            break;
        case FLUX_PLACEMENT_AUTO:
        default:
            pl = FLUX_PLACEMENT_BOTTOM;
            continue;
        }
        break;
    }

    /* Clamp to work area */
    if (x + pw > work.right) x = work.right - pw;
    if (x < work.left)      x = work.left;
    if (y + ph > work.bottom) y = work.bottom - ph;
    if (y < work.top)         y = work.top;

    SetWindowPos(popup->popup_hwnd, HWND_TOPMOST, x, y, pw, ph,
                 SWP_NOACTIVATE);

    flux_graphics_resize(popup->graphics, pw, ph);
}

/* ------- Show / Dismiss ------- */

void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement) {
    if (!popup) return;

    popup->anchor    = anchor;
    popup->placement = placement;
    popup->is_visible = true;

    popup_position(popup);

    ShowWindow(popup->popup_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(popup->popup_hwnd, NULL, FALSE);
}

void flux_popup_dismiss(FluxPopup *popup) {
    if (!popup || !popup->is_visible) return;

    popup->is_visible = false;
    ShowWindow(popup->popup_hwnd, SW_HIDE);

    if (popup->dismiss_cb)
        popup->dismiss_cb(popup->dismiss_ctx);
}

void flux_popup_update_position(FluxPopup *popup) {
    if (!popup || !popup->is_visible) return;
    popup_position(popup);
}

bool flux_popup_is_visible(const FluxPopup *popup) {
    return popup ? popup->is_visible : false;
}

/* ------- Window proc ------- */

static LRESULT CALLBACK popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    FluxPopup *popup = (FluxPopup *)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (popup && popup->graphics) {
            flux_graphics_begin_draw(popup->graphics);
            FluxColor bg = flux_color_rgba(0, 0, 0, 0);
            flux_graphics_clear(popup->graphics, bg);

            /* TODO: render popup content tree here when engine integration is added */

            flux_graphics_end_draw(popup->graphics);
            flux_graphics_present(popup->graphics, true);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ACTIVATE:
        if (popup && popup->dismiss_on_outside && LOWORD(wp) == WA_INACTIVE) {
            flux_popup_dismiss(popup);
        }
        return 0;

    case WM_MOUSEACTIVATE:
        /* Prevent the popup from stealing activation from owner */
        return MA_NOACTIVATE;

    case WM_SIZE:
        if (popup && popup->graphics) {
            int w = LOWORD(lp);
            int h = HIWORD(lp);
            if (w > 0 && h > 0)
                flux_graphics_resize(popup->graphics, w, h);
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}