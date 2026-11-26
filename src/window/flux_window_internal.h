#ifndef FLUX_WINDOW_INTERNAL_H
#define FLUX_WINDOW_INTERNAL_H

#include "fluxent/flux_window.h"

struct FluxText;

#define FLUX_WINDOW_MAX_RESIZE_OBSERVERS 8

#define FLUX_WINDOW_REDRAW_TIMER         0xf2

#define FLUX_WM_APPLY_FRAME              (WM_APP + 0x51)

bool             flux_window_ime_message(FluxWindow *win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);
void             flux_window_setmaxbutton(FluxWindow *win, FluxRect bounds);
bool             flux_window_caption_bounds(FluxWindow *win, FluxRect *bounds);
void             flux_window_redraw_after(FluxWindow *win, double seconds);
void             flux_window_settext(FluxWindow *win, struct FluxText *text);
struct FluxText *flux_window_text(FluxWindow const *win);
void             flux_window_applyframe(FluxWindow *win, bool deferred);
bool             flux_taskbar_message(FluxWindow *win, UINT msg, WPARAM wp);
void             flux_taskbar_destroy(FluxWindow *win);
bool             flux_tray_message(FluxWindow *win, UINT msg, WPARAM wp, LPARAM lp);
void             flux_tray_destroy_window(FluxWindow *win);

typedef struct FluxTaskbar  FluxTaskbar;
typedef struct FluxTrayIcon FluxTrayIcon;

struct FluxWindow {
	HWND                 hwnd;
	HMENU                system_menu;
	FluxDpi              dpi;
	FluxWindowCfg        config;
	FluxGfx             *gfx;
	struct FluxText     *text;
	bool                 render_requested;
	ULONGLONG            redraw_due;
	bool                 shown;
	bool                 ole_initialized;
	int                  cursor_type;
	HCURSOR              cursor_arrow;
	HCURSOR              cursor_ibeam;
	HCURSOR              cursor_hand;
	HCURSOR              cursor_sizewe;
	FluxMenu            *active_menu;
	FluxTaskbar         *taskbar;
	UINT                 taskbar_button_created;
	bool                 taskbar_ready;
	FluxTrayIcon        *tray_icons;
	UINT                 tray_next_id;
	UINT                 taskbar_created;
	HACCEL               accelerators;
	FluxAcceleratorFn    on_accelerator;
	void                *accelerator_ctx;

	uint32_t             primary_touch_pid;
	uint32_t             mouse_buttons;
	float                touch_hold_x;
	float                touch_hold_y;

	FluxRenderFn         on_render;
	void                *render_ctx;
	FluxResizeFn         on_resize;
	void                *resize_ctx;
	FluxResizeFn         resize_observers [FLUX_WINDOW_MAX_RESIZE_OBSERVERS];
	void                *resize_observer_ctx [FLUX_WINDOW_MAX_RESIZE_OBSERVERS];
	int                  resize_observer_count;
	FluxDpiChangedFn     on_dpi_changed;
	void                *dpi_changed_ctx;
	FluxPointerFn        on_pointer;
	void                *pointer_ctx;
	FluxKeyFn            on_key;
	void                *key_ctx;
	bool                 key_repeated;
	FluxCharFn           on_char;
	void                *char_ctx;
	bool                 (*on_syschar)(void *ctx, wchar_t ch);
	void                 (*on_key_cancel)(void *ctx);
	FluxImeCompositionFn on_ime_composition;
	void                *ime_composition_ctx;
	FluxSettingChangedFn on_setting_changed;
	void                *setting_changed_ctx;
	FluxUiaProviderFn    on_uia_provider;
	void                *uia_provider_ctx;

	int                  ime_x;
	int                  ime_y;
	int                  ime_h;
	bool                 ime_position_valid;
	bool                 ime_applying_position;

	bool                 title_bar_active;
	bool                 title_bar_extended;
	bool                 title_bar_native_buttons;
	FluxRect             title_bar_drag;
	FluxRect             title_bar_max;
	bool                 title_bar_max_pressed;
	FluxRect             title_bar_pass [FLUX_WINDOW_MAX_TITLE_BAR_PASS];
	int                  title_bar_pass_count;
};

#endif
