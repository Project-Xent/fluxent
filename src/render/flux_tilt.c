#include "render/flux_tilt.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <windows.h>

#define TILT_MAX_DEPRESSION 0.03f
#define TILT_SIZE_LIMIT     925.0f
#define TILT_HOLD_MS        150.0f
#define TILT_RETURN_MS      200.0f

static float tilt_max_angle(float size) {
	if (size > TILT_SIZE_LIMIT) return 0.0f;
	return -0.00000007f * size * size * size + 0.00015904f * size * size - 0.12506463f * size + 35.27311191f;
}

static void tilt_apply(FluxTilt const *t, float k) {
	FluxNodeData *nd = flux_store_find(t->store, t->node);
	if (!nd) return;
	nd->render_scale    = 1.0f - (1.0f - t->scale) * k;
	nd->render_rotate_x = t->rotate_x * k;
	nd->render_rotate_y = t->rotate_y * k;
	HWND hwnd           = t->window ? flux_window_hwnd(t->window) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

void flux_tilt_press(FluxTilt *t, XentNodeId node, float x, float y) {
	XentRect r = {0};
	flux_tilt_stop(t);
	if (!xent_layout_rect(flux_store_ctx(t->store), node, &r) || r.w <= 0.0f || r.h <= 0.0f) return;
	float nx    = fminf(fmaxf(x / r.w, 0.0f), 1.0f);
	float ny    = fminf(fmaxf(y / r.h, 0.0f), 1.0f);
	t->node     = node;
	t->scale    = 1.0f - TILT_MAX_DEPRESSION * (1.0f - fabsf(nx - 0.5f) - fabsf(ny - 0.5f));
	t->rotate_x = tilt_max_angle(r.h) * (ny - 0.5f);
	t->rotate_y = -tilt_max_angle(r.w) * (nx - 0.5f);
	tilt_apply(t, 1.0f);
}

static bool tilt_step(void *ctx, unsigned long now) {
	FluxTilt *t       = ( FluxTilt * ) ctx;
	float     elapsed = ( float ) (now - t->start) - TILT_HOLD_MS;
	if (elapsed < 0.0f) return true;
	float p = fminf(elapsed / TILT_RETURN_MS, 1.0f);
	tilt_apply(t, 1.0f - flux_cubic_bezier(p, 0.1f, 0.25f, 0.75f, 0.9f));
	if (p < 1.0f) return true;
	t->node = XENT_NODE_INVALID;
	return false;
}

void flux_tilt_release(FluxTilt *t) {
	if (t->node == XENT_NODE_INVALID) return;
	t->start = flux_anim_now();
	flux_anim_register(t, tilt_step);
}

void flux_tilt_stop(FluxTilt *t) {
	flux_anim_unregister(t);
	if (t->node != XENT_NODE_INVALID) tilt_apply(t, 0.0f);
	t->node = XENT_NODE_INVALID;
}
