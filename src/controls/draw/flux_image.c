#include "render/flux_fluent.h"
#include "render/flux_image_cache.h"

static FluxRect image_dest(FluxRect const *b, float nw, float nh, FluxImageStretch stretch) {
	if (nw <= 0.0f || nh <= 0.0f || stretch == FLUX_IMAGE_FILL) return *b;
	if (stretch == FLUX_IMAGE_NONE) return (FluxRect) {b->x + (b->w - nw) * 0.5f, b->y + (b->h - nh) * 0.5f, nw, nh};

	float sx = b->w / nw;
	float sy = b->h / nh;
	float s  = stretch == FLUX_IMAGE_UNIFORM ? flux_minf(sx, sy) : flux_maxf(sx, sy);
	float w  = nw * s;
	float h  = nh * s;
	return (FluxRect) {b->x + (b->w - w) * 0.5f, b->y + (b->h - h) * 0.5f, w, h};
}

void flux_draw_image(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	if (!snap->text_content || !rc->d2d) return;

	float nw = 0.0f, nh = 0.0f;
	void *bmp = flux_image_cache_acquire(rc->d2d, snap->text_content, &nw, &nh);
	if (!bmp) return;

	FluxRect    sb   = flux_snap_bounds(bounds, 1.0f, 1.0f);
	FluxRect    dest = image_dest(&sb, nw, nh, ( FluxImageStretch ) ( int ) (snap->current_value + 0.5f));

	D2D1_RECT_F clip = flux_d2d_rect(&sb);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	D2D1_RECT_F d = {dest.x, dest.y, dest.x + dest.w, dest.y + dest.h};
	ID2D1DeviceContext_DrawBitmap(
	  rc->d2d, ( ID2D1Bitmap * ) bmp, &d, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL
	);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}
