#ifndef FLUX_TASKBAR_H
#define FLUX_TASKBAR_H

#include "flux_window.h"

typedef enum FluxTaskbarProgress
{
	FLUX_TASKBAR_PROGRESS_NONE,
	FLUX_TASKBAR_PROGRESS_INDETERMINATE,
	FLUX_TASKBAR_PROGRESS_NORMAL,
	FLUX_TASKBAR_PROGRESS_PAUSED,
	FLUX_TASKBAR_PROGRESS_ERROR,
} FluxTaskbarProgress;

#define FLUX_TASKBAR_MAX_BUTTONS 7u

typedef struct FluxTaskbarButton {
	HICON          icon;
	wchar_t const *tooltip;
	bool           disabled;
} FluxTaskbarButton;

typedef void (*FluxTaskbarButtonFn)(void *ctx, uint32_t index);

/** Returns S_FALSE when the taskbar button is pending. The latest state is applied when Explorer creates it. */
HRESULT      flux_taskbar_set_progress(FluxWindow *win, FluxTaskbarProgress state, uint64_t completed, uint64_t total);
/** Copies the icon and description. A null icon clears the overlay. */
HRESULT      flux_taskbar_set_overlay(FluxWindow *win, HICON icon, wchar_t const *description);
/** Copies the buttons, icons and tooltips. Borrows ctx until replaced or the window is destroyed. */
HRESULT      flux_taskbar_set_buttons(
  FluxWindow *win, FluxTaskbarButton const *buttons, uint32_t count, FluxTaskbarButtonFn callback, void *ctx
);

#endif
