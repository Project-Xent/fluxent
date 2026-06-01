#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_shadow.h"
#include "compose/flux_interop.h"

#include <cwinrt/cast.h>

static WUI_Color shadow_wui_color(FluxColor c) {
	WUI_Color col;
	col.A = ( uint8_t ) (flux_color_af(c) * 255.0f + 0.5f);
	col.R = ( uint8_t ) (flux_color_rf(c) * 255.0f + 0.5f);
	col.G = ( uint8_t ) (flux_color_gf(c) * 255.0f + 0.5f);
	col.B = ( uint8_t ) (flux_color_bf(c) * 255.0f + 0.5f);
	return col;
}

WUC_DropShadow *
flux_shadow_apply(FluxCompositor *c, WUC_Sprite *sprite, FluxColor color, float blur, float dx, float dy) {
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp || !sprite) return NULL;

	WUC_DropShadow *shadow = NULL;
	if (FAILED(wuc_comp_create_drop_shadow(comp, &shadow))) return NULL;

	( void ) wuc_drop_shadow_put__color(shadow, shadow_wui_color(color));
	( void ) wuc_drop_shadow_put__blur_radius(shadow, blur);
	WFN_Vector_3 off = {dx, dy, 0.0f};
	( void ) wuc_drop_shadow_put__offset(shadow, off);

	WUC_CompositionShadow *base = NULL;
	if (SUCCEEDED(cwinrt_query(shadow, &CWINRT_IID_WUC_ICompositionShadow, ( void ** ) &base))) {
		( void ) wuc_sprite_put__shadow(sprite, base);
		(( IUnknown * ) base)->lpVtbl->Release(( IUnknown * ) base);
	}
	return shadow;
}

void flux_shadow_destroy(WUC_DropShadow *shadow) {
	if (shadow) (( IUnknown * ) shadow)->lpVtbl->Release(( IUnknown * ) shadow);
}
