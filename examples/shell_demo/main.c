#include <fluxent/fluxent.h>

#include <stdio.h>

typedef enum DemoAction
{
	DEMO_ADVANCE = 1,
	DEMO_PAUSE,
	DEMO_CLEAR,
	DEMO_TRAY,
	DEMO_TOAST,
} DemoAction;

typedef struct ShellDemo {
	FluxApp      *app;
	FluxTrayIcon *tray;
	HMENU         menu;
	unsigned      progress;
	bool          paused;
	char          status [96];
} ShellDemo;

static void demo_taskbar_button(void *ctx, uint32_t index) {
	ShellDemo *demo = ctx;
	( void ) flux_app_post(demo->app, xtk_msg(index == 0 ? DEMO_ADVANCE : DEMO_PAUSE));
}

static void demo_tray_activate(void *ctx) {
	ShellDemo *demo = ctx;
	HWND       hwnd = flux_app_hwnd(demo->app);
	ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);
}

static void demo_tray_command(void *ctx, uint32_t command) {
	ShellDemo *demo = ctx;
	if (command == 1u) demo_tray_activate(ctx);
	if (command == 2u) ( void ) flux_app_post(demo->app, xtk_msg(DEMO_ADVANCE));
}

static void demo_accelerator(void *ctx, uint16_t command) {
	ShellDemo *demo = ctx;
	( void ) flux_app_post(demo->app, xtk_msg(command));
}

static HRESULT demo_toggle_tray(ShellDemo *demo) {
	if (demo->tray) {
		flux_tray_destroy(demo->tray);
		demo->tray = NULL;
		return S_OK;
	}
	FluxTrayDesc desc = {
	  .icon     = LoadIconW(NULL, IDI_APPLICATION),
	  .tooltip  = L"FluXent shell demo",
	  .menu     = demo->menu,
	  .activate = demo_tray_activate,
	  .command  = demo_tray_command,
	  .ctx      = demo};
	return flux_tray_create(flux_app_window(demo->app), &desc, &demo->tray);
}

static HRESULT demo_toast(void) {
	wchar_t app_id [128];
	DWORD   length = GetEnvironmentVariableW(L"FLUXENT_SHELL_DEMO_APP_ID", app_id, ARRAYSIZE(app_id));
	if (length == 0 || length >= ARRAYSIZE(app_id)) return E_INVALIDARG;
	return flux_toast_show(
	  app_id,
	  L"<toast><visual><binding template=\"ToastGeneric\"><text>FluXent shell demo</text>"
	  L"<text>Windows toast notification</text></binding></visual></toast>"
	);
}

static HRESULT demo_progress(ShellDemo *demo, int tag) {
	FluxWindow *window = flux_app_window(demo->app);
	if (tag == DEMO_CLEAR) {
		demo->progress = 0;
		demo->paused   = false;
		( void ) flux_taskbar_set_overlay(window, NULL, NULL);
		return flux_taskbar_set_progress(window, FLUX_TASKBAR_PROGRESS_NONE, 0, 0);
	}
	if (tag == DEMO_ADVANCE) demo->progress = (demo->progress + 20u) % 120u;
	if (tag == DEMO_PAUSE) demo->paused = !demo->paused;
	HICON icon = demo->paused ? LoadIconW(NULL, IDI_WARNING) : NULL;
	( void ) flux_taskbar_set_overlay(window, icon, L"Paused");
	return flux_taskbar_set_progress(
	  window, demo->paused ? FLUX_TASKBAR_PROGRESS_PAUSED : FLUX_TASKBAR_PROGRESS_NORMAL, demo->progress, 100u
	);
}

static void demo_update(void *ctx, XtkMsg msg) {
	ShellDemo *demo = ctx;
	HRESULT    hr   = S_OK;
	switch (msg.tag) {
	case DEMO_TRAY  : hr = demo_toggle_tray(demo); break;
	case DEMO_TOAST : hr = demo_toast(); break;
	default         : hr = demo_progress(demo, msg.tag); break;
	}
	snprintf(
	  demo->status, sizeof(demo->status), "Progress %u%%  Windows result 0x%08lX", demo->progress, ( unsigned long ) hr
	);
}

static XtkEl *demo_view(XtkUi *ui, void *ctx) {
	ShellDemo const *demo = ctx;
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 8, .padding = {20, 20, 20, 20}
    },
	  xtk_kids(
		xtk_automation(
		  ui, xtk_button(ui, "Advance taskbar progress", (XtkButtonDesc) {.click = xtk_msg(DEMO_ADVANCE)}),
		  &(XtkAutomation) {.access_key = "N"}
		),
		xtk_button(ui, "Pause or resume progress", (XtkButtonDesc) {.click = xtk_msg(DEMO_PAUSE)}),
		xtk_button(ui, "Clear taskbar progress", (XtkButtonDesc) {.click = xtk_msg(DEMO_CLEAR)}),
		xtk_button(ui, "Toggle tray icon", (XtkButtonDesc) {.click = xtk_msg(DEMO_TRAY)}),
		xtk_button(ui, "Send toast", (XtkButtonDesc) {.click = xtk_msg(DEMO_TOAST)}),
		xtk_text(ui, demo->status, (XtkTextDesc) {0})
	  )
	);
}

static void demo_start(FluxApp *app, void *ctx) {
	ShellDemo *demo = ctx;
	demo->app       = app;
	demo->menu      = CreatePopupMenu();
	if (demo->menu) {
		AppendMenuW(demo->menu, MF_STRING, 1, L"Show window");
		AppendMenuW(demo->menu, MF_STRING, 2, L"Advance progress");
	}
	FluxTaskbarButton const buttons [] = {
	  {.icon = LoadIconW(NULL, IDI_INFORMATION), .tooltip = L"Advance progress"},
	  {.icon = LoadIconW(NULL, IDI_WARNING),     .tooltip = L"Pause or resume" }
    };
	( void ) flux_taskbar_set_buttons(flux_app_window(app), buttons, ARRAYSIZE(buttons), demo_taskbar_button, demo);
	ACCEL const keys [] = {
	  {FVIRTKEY | FCONTROL, 'P', DEMO_ADVANCE}
    };
	( void ) flux_window_setaccelerators(flux_app_window(app), keys, ARRAYSIZE(keys), demo_accelerator, demo);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show) {
	( void ) instance;
	( void ) previous;
	( void ) args;
	( void ) show;
	ShellDemo  demo = {.status = "Progress 0%"};
	FluxAppCfg cfg
	  = {.title = L"FluXent shell demo", .width = 460, .height = 340, .on_start = demo_start, .user = &demo};
	flux_controls_all();
	int code = flux_run(&cfg, &demo, demo_update, demo_view);
	if (demo.menu) DestroyMenu(demo.menu);
	return code;
}
