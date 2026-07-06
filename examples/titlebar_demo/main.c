/**
 * @file main.c
 * @brief Extended-title-bar demo: a window with the OS caption removed, whose
 * title bar (icon, title, subtitle, back + pane-toggle, and min/max/close
 * caption buttons) is drawn by a window-integrated FluxTitleBar. Dragging the
 * band moves the window; the caption buttons minimize / maximize / close it.
 */
#include <fluxent/fluxent.h>
#include <fluxent/flux_app.h>

#include <math.h>

enum
{
	MSG_NONE = 0,
	MSG_BACK,
	MSG_PANE,
};

typedef struct DemoModel {
	int back_clicks;
	int pane_clicks;
} DemoModel;

static void update(void *model, XtkMsg msg) {
	DemoModel *m = ( DemoModel * ) model;
	if (msg.tag == MSG_BACK) m->back_clicks++;
	else if (msg.tag == MSG_PANE) m->pane_clicks++;
}

static XtkEl *view(XtkUi *ui, void *model) {
	DemoModel *m = ( DemoModel * ) model;
	/* The scene root sizes children to content, so a fixed-width body pins the
	 * whole column (and thus the stretched title bar) to the window's width. */
	return xtk_column(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		xtk_title_bar(
		  ui, "Fluxent TitleBar Demo",
		  (XtkTitleBarDesc) {
			.subtitle         = "Preview",
			.icon             = "Home",
			.show_back        = true,
			.show_pane_toggle = true,
			.integrate_window = true,
			.on_back          = xtk_msg(MSG_BACK),
			.on_pane_toggle   = xtk_msg(MSG_PANE),
		  }
		),
		xtk_column(
		  ui, (XtkStackDesc) {.padding = {28, 24, 28, 24}, .gap = 12, .align = XENT_FLEX_ALIGN_START},
		  (XtkEl *[]) {
			xtk_text(
			  ui, "This window has no system title bar.", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}
			),
			xtk_text(ui, "The band above is drawn by the app.", (XtkTextDesc) {.size = 14}),
			xtk_text(ui, "Drag it to move the window.", (XtkTextDesc) {.size = 14}),
			xtk_text(ui, "The buttons at the right minimize, maximize, close.", (XtkTextDesc) {.size = 14}),
			xtk_text(
			  ui, xtk_fmt(ui, "Back clicks: %d   Pane-toggle clicks: %d", m->back_clicks, m->pane_clicks),
			  (XtkTextDesc) {.size = 13}
			),
			XTK_END}
		),
		XTK_END}
	);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	static DemoModel model = {0};
	FluxAppConfig    cfg    = {
	     .title     = L"Fluxent TitleBar Demo",
	     .width     = 900,
	     .height    = 600,
	     .dark_mode = flux_theme_system_is_dark(),
	     .backdrop  = FLUX_BACKDROP_MICA,
	};
	return flux_run(&cfg, &model, update, view);
}
