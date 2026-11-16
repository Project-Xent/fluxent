#include "render/flux_projection.h"

#include <math.h>
#include <string.h>

#define PROJECTION_CAMERA_Z 999.0f

static void release(void *unknown) {
	if (unknown) (( IUnknown * ) unknown)->lpVtbl->Release(( IUnknown * ) unknown);
}

static ID2D1DeviceContext *recorder(ID2D1DeviceContext *parent) {
	ID2D1Device        *device = NULL;
	ID2D1DeviceContext *dc     = NULL;
	ID2D1DeviceContext_GetDevice(parent, &device);
	if (!device) return NULL;
	if (FAILED(ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) dc = NULL;
	release(device);
	return dc;
}

bool flux_projection_begin(
  FluxProjection *p, FluxRenderCtx const *parent, D2D1_MATRIX_3X2_F const *content, float cx, float cy, float rotate_x,
  float rotate_y
) {
	ID2D1DeviceContext *dc   = recorder(parent->d2d);
	ID2D1CommandList   *list = NULL;
	if (!dc || FAILED(ID2D1DeviceContext_CreateCommandList(dc, &list))) {
		release(dc);
		return false;
	}
	float dpi_x = 96.0f, dpi_y = 96.0f;
	ID2D1DeviceContext_GetDpi(parent->d2d, &dpi_x, &dpi_y);
	ID2D1DeviceContext_SetDpi(dc, dpi_x, dpi_y);
	ID2D1DeviceContext_SetTarget(dc, ( ID2D1Image * ) list);
	ID2D1DeviceContext_BeginDraw(dc);
	ID2D1DeviceContext_SetTextAntialiasMode(dc, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	D2D1_MATRIX_3X2_F local  = *content;
	local._31               -= cx;
	local._32               -= cy;
	ID2D1DeviceContext_SetTransform(dc, &local);
	*p          = (FluxProjection) {.dc = dc, .list = list, .rc = *parent, .cx = cx, .cy = cy};
	p->rc.d2d   = dc;
	p->rotate_x = rotate_x;
	p->rotate_y = rotate_y;
	return true;
}

static void mat4_mul(float const a [16], float const b [16], float out [16]) {
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++)
			out [r * 4 + c]
			  = a [r * 4] * b [c] + a [r * 4 + 1] * b [4 + c] + a [r * 4 + 2] * b [8 + c] + a [r * 4 + 3] * b [12 + c];
}

/* Row-vector convention (D2D): x right, y down, z toward the viewer. RotationX > 0 sends the bottom edge
   away, RotationY > 0 brings the right edge closer; X is applied before Y, as in PlaneProjection. */
static void projection_matrix(float rotate_x, float rotate_y, float out [16]) {
	float b       = rotate_x * 3.14159265f / 180.0f;
	float a       = rotate_y * 3.14159265f / 180.0f;
	float rx [16] = {1, 0, 0, 0, 0, cosf(b), -sinf(b), 0, 0, sinf(b), cosf(b), 0, 0, 0, 0, 1};
	float ry [16] = {cosf(a), 0, sinf(a), 0, 0, 1, 0, 0, -sinf(a), 0, cosf(a), 0, 0, 0, 0, 1};
	float pr [16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1.0f / PROJECTION_CAMERA_Z, 0, 0, 0, 1};
	float rot [16];
	mat4_mul(rx, ry, rot);
	mat4_mul(rot, pr, out);
}

static void draw_projected(FluxProjection const *p, FluxRenderCtx const *parent) {
	ID2D1Effect *fx  = NULL;
	ID2D1Image  *out = NULL;
	if (FAILED(ID2D1DeviceContext_CreateEffect(parent->d2d, &CLSID_D2D13DTransform, &fx))) return;
	float m [16];
	projection_matrix(p->rotate_x, p->rotate_y, m);
	ID2D1Effect_SetInput(fx, 0, ( ID2D1Image * ) p->list, TRUE);
	ID2D1Effect_SetValue(
	  fx, D2D1_3DTRANSFORM_PROP_TRANSFORM_MATRIX, D2D1_PROPERTY_TYPE_MATRIX_4X4, ( UINT8 const * ) m, sizeof(m)
	);
	ID2D1Effect_GetOutput(fx, &out);
	D2D1_MATRIX_3X2_F saved;
	D2D1_MATRIX_3X2_F identity = {0};
	identity._11               = 1.0f;
	identity._22               = 1.0f;
	D2D_POINT_2F at            = {p->cx, p->cy};
	ID2D1DeviceContext_GetTransform(parent->d2d, &saved);
	ID2D1DeviceContext_SetTransform(parent->d2d, &identity);
	if (out)
		ID2D1DeviceContext_DrawImage(
		  parent->d2d, out, &at, NULL, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER
		);
	ID2D1DeviceContext_SetTransform(parent->d2d, &saved);
	release(out);
	release(fx);
}

void flux_projection_end(FluxProjection *p, FluxRenderCtx const *parent) {
	ID2D1DeviceContext_EndDraw(p->dc, NULL, NULL);
	ID2D1DeviceContext_SetTarget(p->dc, NULL);
	if (SUCCEEDED(ID2D1CommandList_Close(p->list))) draw_projected(p, parent);
	release(p->list);
	release(p->dc);
	memset(p, 0, sizeof(*p));
}
