#ifndef FLUX_WINDOW_INTERNAL_H
#define FLUX_WINDOW_INTERNAL_H

#include "fluxent/flux_window.h"

#define FLUX_WINDOW_MAX_RESIZE_OBSERVERS 8

/** @brief Private message: apply a deferred frame change (extend-into-title-bar).
 *  Posted so the SWP_FRAMECHANGED runs after the current frame instead of
 *  re-entering WM_SIZE/render mid-reconcile (which double-composites the scene). */
#define FLUX_WM_APPLY_FRAME (WM_APP + 0x51)

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
	float                      touch_hold_x; /**< Press-and-hold anchor (logical client px). */
	float                      touch_hold_y;

	FluxRenderCallback         on_render;
	void                      *render_ctx;
	FluxResizeCallback         on_resize;
	void                      *resize_ctx;
	FluxResizeCallback         resize_observers [FLUX_WINDOW_MAX_RESIZE_OBSERVERS];
	void                      *resize_observer_ctx [FLUX_WINDOW_MAX_RESIZE_OBSERVERS];
	int                        resize_observer_count;
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

	bool                       title_bar_active;                                    /**< A custom title bar registered a drag region. */
	bool                       title_bar_extended;                                  /**< OS caption removed; client covers the title bar. */
	FluxRect                   title_bar_drag;                                      /**< Drag region (client-local DIPs). */
	FluxRect                   title_bar_pass [FLUX_WINDOW_MAX_TITLE_BAR_PASS];     /**< Interactive passthrough rects (DIPs). */
	int                        title_bar_pass_count;
};

/** @brief Handle WM_GETOBJECT via the UIA provider callback; 0 if not applicable. */
LRESULT flux_window_uia_get_object(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp);

#endif
