#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <fluxent/flux_node_store.h>
#include "render/flux_render_snapshot.h"
#include <fluxent/flux_theme.h>
#include <xent/xent_display.h>

#include "controls/draw/flux_control_draw.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_internal.h"
#include "render/flux_shape_resource.h"

#include <windows.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static int test_record_container_owned(void) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type               = FLUX_CONTROL_CONTAINER;
	snap.background         = flux_color_rgba(32, 64, 128, 255);
	snap.border_color       = flux_color_rgba(0, 0, 0, 40);
	snap.border_width       = 1.0f;
	snap.corner_radius      = 4.0f;
	FluxRect         bounds = {0.0f, 0.0f, 120.0f, 40.0f};
	FluxControlState state  = {.enabled = true};
	XentDl          *list   = flux_container_record(NULL, &snap, &bounds, &state);
	EXPECT(list != NULL, "record container");
	EXPECT(xent_dl_ncmd(list) >= 2u, "fill+stroke cmds");

	FluxStore *store = flux_store_create(8u);
	EXPECT(store != NULL, "store");
	XentCtx *ctx = xent_ctx_create(NULL);
	EXPECT(ctx != NULL, "ctx");
	EXPECT(flux_store_bind(store, ctx), "bind");
	XentNodeId node = xent_node_create(ctx);
	EXPECT(node != XENT_NODE_INVALID, "node");

	flux_node_adoptdl(store, node, list);
	EXPECT(flux_node_dl(store, node) == list, "attached");

	XentDl *second = flux_divider_record(NULL, &snap, &bounds, &state);
	EXPECT(second != NULL, "record divider");
	flux_node_adoptdl(store, node, second);
	EXPECT(flux_node_dl(store, node) == second, "replaced");

	flux_node_setdl(store, node, NULL);
	EXPECT(flux_node_dl(store, node) == NULL, "cleared borrow");

	flux_store_destroy(store);
	xent_ctx_destroy(ctx);
	return 0;
}

static int test_record_text(void) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type                = FLUX_CONTROL_TEXT;
	snap.u.text.text_content = "hello";
	snap.font_size           = 14.0f;
	FluxRect         bounds  = {0.0f, 0.0f, 80.0f, 24.0f};
	FluxControlState state   = {.enabled = true};
	XentDl          *list    = flux_text_record(NULL, &snap, &bounds, &state);
	EXPECT(list != NULL, "record text");
	EXPECT(xent_dl_ncmd(list) >= 1u, "text cmd");
	xent_dl_free(list);
	return 0;
}

static int test_progress_nonzero_min(void) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type                     = FLUX_CONTROL_PROGRESS;
	snap.u.progress.min_value     = 25.0f;
	snap.u.progress.current_value = 75.0f;
	snap.u.progress.max_value     = 125.0f;
	FluxRect         bounds       = {0.0f, 0.0f, 200.0f, 4.0f};
	FluxControlState state        = {.enabled = true};
	XentDl          *list         = flux_progress_record(NULL, &snap, &bounds, &state);
	EXPECT(list != NULL, "record ranged progress");

	XentDlCmd const *fill = NULL;
	for (uint32_t i = 0; i < xent_dl_ncmd(list); i++) {
		XentDlCmd const *cmd = xent_dl_cmd(list, i);
		if (cmd && cmd->op == XENT_DL_FILLRRECT && cmd->u.geom.rect.h == 3.0f) fill = cmd;
	}
	EXPECT(fill != NULL, "ranged progress fill present");
	EXPECT(fill->u.geom.rect.w == 100.0f, "ranged progress normalizes from minimum");
	xent_dl_free(list);
	return 0;
}

static int test_tab_selected_fill_shape(void) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type                  = FLUX_CONTROL_TAB_VIEW_ITEM;
	snap.u.tab.tab_kind        = 0; /* FLUX_TAB_KIND_TAB */
	snap.u.tab.is_checked      = true;
	snap.u.tab.label           = "Home";
	FluxControlState state     = {.enabled = true};
	float            widths [] = {80.0f, 160.0f, 240.0f};
	for (size_t i = 0; i < sizeof(widths) / sizeof(widths [0]); i++) {
		FluxRect bounds = {0.0f, 0.0f, widths [i], 32.0f};
		XentDl  *list   = flux_tabitem_record(NULL, &snap, &bounds, &state);
		EXPECT(list != NULL, "record selected tab");
		XentDlCmd const *cmd = NULL;
		for (uint32_t j = 0; j < xent_dl_ncmd(list); j++) {
			XentDlCmd const *c = xent_dl_cmd(list, j);
			if (c && c->op == XENT_DL_FILLSHAPE) {
				cmd = c;
				break;
			}
		}
		EXPECT(cmd != NULL, "FILL_SHAPE present");
		EXPECT(cmd->u.shape.shape == FLUX_SHAPE_TAB_SELECTED_FRAME, "same shape id");
		EXPECT(cmd->u.shape.bounds.w == widths [i], "bounds width preserved");
		EXPECT(FLUX_TAB_SELECTED_CORNER == 8.0f, "corner fixed 8");
		EXPECT(FLUX_TAB_SELECTED_FLARE == 4.0f, "flare fixed 4");
		xent_dl_free(list);
	}
	return 0;
}

typedef struct PixelTarget {
	ID2D1Factory         *factory;
	ID2D1DCRenderTarget  *target;
	ID2D1SolidColorBrush *brush;
	HDC                   dc;
	HBITMAP               bitmap;
	HGDIOBJ               old_bitmap;
	uint32_t             *pixels;
} PixelTarget;

static void pixel_target_destroy(PixelTarget *target) {
	if (target->brush) ID2D1SolidColorBrush_Release(target->brush);
	if (target->target) ID2D1DCRenderTarget_Release(target->target);
	if (target->factory) ID2D1Factory_Release(target->factory);
	if (target->dc && target->old_bitmap) SelectObject(target->dc, target->old_bitmap);
	if (target->bitmap) DeleteObject(target->bitmap);
	if (target->dc) DeleteDC(target->dc);
}

static bool pixel_target_create(PixelTarget *target, int width, int height) {
	memset(target, 0, sizeof(*target));
	D2D1_FACTORY_OPTIONS options = {0};
	if (FAILED(D2D1CreateFactory(
		  D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, &options, ( void ** ) &target->factory
		)))
		return false;

	D2D1_RENDER_TARGET_PROPERTIES props = {
	  D2D1_RENDER_TARGET_TYPE_SOFTWARE,
	  {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
	  96.0f,
	  96.0f,
	  D2D1_RENDER_TARGET_USAGE_NONE,
	  D2D1_FEATURE_LEVEL_DEFAULT,
	};
	if (FAILED(ID2D1Factory_CreateDCRenderTarget(target->factory, &props, &target->target))) return false;

	BITMAPINFO info              = {0};
	info.bmiHeader.biSize        = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth       = width;
	info.bmiHeader.biHeight      = -height;
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;
	target->dc                   = CreateCompatibleDC(NULL);
	target->bitmap = CreateDIBSection(target->dc, &info, DIB_RGB_COLORS, ( void ** ) &target->pixels, NULL, 0u);
	if (!target->dc || !target->bitmap || !target->pixels) return false;
	target->old_bitmap = SelectObject(target->dc, target->bitmap);
	RECT rect          = {0, 0, width, height};
	if (FAILED(ID2D1DCRenderTarget_BindDC(target->target, target->dc, &rect))) return false;

	D2D1_COLOR_F black = {0.0f, 0.0f, 0.0f, 1.0f};
	return SUCCEEDED(
	  ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) target->target, &black, NULL, &target->brush)
	);
}

static int test_textbox_focus_accent_pixels(void) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type               = FLUX_CONTROL_TEXT_INPUT;
	snap.corner_radius      = 4.0f;
	FluxControlState state  = {.enabled = true, .focused = true};
	FluxRect         bounds = {0.0f, 0.0f, 120.0f, 32.0f};
	FluxRenderCtx    rc     = {
	  .dpi   = {96.0f, 96.0f},
	  .theme = flux_theme_defaults(),
	};
	XentDl *list = flux_textbox_record(&rc, &snap, &bounds, &state);
	EXPECT(list != NULL, "record focused textbox");
	XentDlCmd const *accent = NULL;
	for (uint32_t i = 0; i < xent_dl_ncmd(list); i++) {
		XentDlCmd const *cmd = xent_dl_cmd(list, i);
		if (cmd && cmd->op == XENT_DL_STROKELINE && cmd->u.line.y0 == 31.0f) accent = cmd;
	}
	EXPECT(accent != NULL, "focused accent line present");
	EXPECT(accent->u.line.stroke_width == 2.0f, "focused accent is two DIPs wide");

	PixelTarget target;
	EXPECT(pixel_target_create(&target, 120, 32), "create textbox pixel target");
	rc.d2d   = ( ID2D1DeviceContext * ) target.target;
	rc.brush = target.brush;
	ID2D1RenderTarget_BeginDraw(( ID2D1RenderTarget * ) target.target);
	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) target.target, &(D2D1_COLOR_F) {0});
	EXPECT(flux_xtk_replay_rc(&rc, list), "replay focused textbox");
	EXPECT(
	  SUCCEEDED(ID2D1RenderTarget_EndDraw(( ID2D1RenderTarget * ) target.target, NULL, NULL)), "finish textbox pixels"
	);

	uint32_t row30 = target.pixels [30u * 120u + 60u];
	uint32_t row31 = target.pixels [31u * 120u + 60u];
	uint8_t  red30 = ( uint8_t ) (row30 >> 16u), blue30 = ( uint8_t ) row30;
	uint8_t  red31 = ( uint8_t ) (row31 >> 16u), blue31 = ( uint8_t ) row31;
	EXPECT(blue30 > red30 + 20u, "focused accent covers bottom row - 2");
	EXPECT(blue31 > red31 + 20u, "focused accent covers bottom row - 1");

	pixel_target_destroy(&target);
	xent_dl_free(list);
	return 0;
}

static uint32_t replay_button_style(PixelTarget *target, FluxRenderCtx *rc, FluxButtonStyle style) {
	FluxRenderSnap snap;
	memset(&snap, 0, sizeof(snap));
	snap.type                  = FLUX_CONTROL_BUTTON;
	snap.u.button.button_style = style;
	FluxControlState state     = {.enabled = true};
	FluxRect         bounds    = {0.0f, 0.0f, 64.0f, 32.0f};
	XentDl          *list      = flux_button_record(rc, &snap, &bounds, &state);
	if (!list) return 0u;

	ID2D1RenderTarget_BeginDraw(( ID2D1RenderTarget * ) target->target);
	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) target->target, &(D2D1_COLOR_F) {0});
	bool    replayed = flux_xtk_replay_rc(rc, list);
	HRESULT drawn    = ID2D1RenderTarget_EndDraw(( ID2D1RenderTarget * ) target->target, NULL, NULL);
	xent_dl_free(list);
	if (!replayed || FAILED(drawn)) return 0u;
	return target->pixels [16u * 64u + 32u];
}

static int test_button_style_pixels(void) {
	PixelTarget target;
	EXPECT(pixel_target_create(&target, 64, 32), "create button pixel target");
	FluxRenderCtx rc = {
	  .d2d   = ( ID2D1DeviceContext * ) target.target,
	  .brush = target.brush,
	  .dpi   = {96.0f, 96.0f},
	  .theme = flux_theme_defaults(),
	};

	uint32_t standard = replay_button_style(&target, &rc, FLUX_BUTTON_STANDARD);
	uint32_t subtle   = replay_button_style(&target, &rc, FLUX_BUTTON_SUBTLE);
	uint32_t text     = replay_button_style(&target, &rc, FLUX_BUTTON_TEXT);
	uint32_t accent   = replay_button_style(&target, &rc, FLUX_BUTTON_ACCENT);
	EXPECT((standard >> 24u) != 0u, "standard button paints chrome");
	EXPECT(subtle == 0u && text == 0u, "subtle and text button chrome is transparent at rest");
	EXPECT((accent >> 24u) == 0xffu, "accent button paints opaque chrome");
	EXPECT((accent & 0xffu) > ((accent >> 16u) & 0xffu) + 100u, "accent button replays blue fill");

	pixel_target_destroy(&target);
	return 0;
}

int main(void) {
	if (test_record_container_owned()) return 1;
	if (test_record_text()) return 1;
	if (test_progress_nonzero_min()) return 1;
	if (test_tab_selected_fill_shape()) return 1;
	if (test_textbox_focus_accent_pixels()) return 1;
	if (test_button_style_pixels()) return 1;
	printf("PASS: flux display list record/ownership\n");
	return 0;
}
