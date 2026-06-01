#ifndef FLUX_WINDOW_INTERNAL_H
#define FLUX_WINDOW_INTERNAL_H

#include "fluxent/flux_window.h"

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
	FluxMenuFlyout            *active_menu;

	uint32_t                   primary_touch_pid;
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
	FluxUiaProviderCallback    on_uia_provider;
	void                      *uia_provider_ctx;

	int                        ime_x;
	int                        ime_y;
	int                        ime_h;
	bool                       ime_position_valid;
	bool                       ime_applying_position;
};

/** @brief Handle WM_GETOBJECT via the UIA provider callback; 0 if not applicable. */
LRESULT flux_window_uia_get_object(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp);

#endif
