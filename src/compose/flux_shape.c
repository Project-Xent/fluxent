#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_shape.h"
#include "compose/flux_interop.h"

#include <cwinrt/cast.h>

#include <stdlib.h>

struct FluxShapeRect {
	WUC_Comp                                *c;
	WUC_ShapeVisual                         *visual;
	WUC_Visual                              *base;
	WUC_CompositionRoundedRectangleGeometry *geometry;
	WUC_CompositionSpriteShape              *shape;
	WUC_CompositionColorBrush               *fill;
	WUC_CompositionColorBrush               *stroke;
};

static WUC_Brush *color_brush_base(WUC_CompositionColorBrush *cb) {
	WUC_Brush *base = NULL;
	( void ) cwinrt_query(cb, &CWINRT_IID_WUC_ICompositionBrush, ( void ** ) &base);
	return base;
}

static bool shape_build_geometry(FluxShapeRect *s, WUC_Comp *comp, float w, float h, float radius) {
	if (FAILED(wuc_comp_create_rounded_rectangle_geometry(comp, &s->geometry))) return false;
	WFN_Vector_2 size = {w, h};
	( void ) wuc_composition_rounded_rectangle_geometry_put__size(s->geometry, size);
	WFN_Vector_2 cr = {radius, radius};
	( void ) wuc_composition_rounded_rectangle_geometry_put__corner_radius(s->geometry, cr);

	WUC_CompositionGeometry *geo_base = NULL;
	if (FAILED(cwinrt_query(s->geometry, &CWINRT_IID_WUC_ICompositionGeometry, ( void ** ) &geo_base))) return false;
	HRESULT hr = wuc_comp_create_sprite_shape_p_p(comp, geo_base, &s->shape);
	(( IUnknown * ) geo_base)->lpVtbl->Release(( IUnknown * ) geo_base);
	return SUCCEEDED(hr);
}

static void shape_set_fill(FluxShapeRect *s, WUC_Comp *comp) {
	WUI_Color black = {255, 0, 0, 0};
	if (FAILED(wuc_comp_create_color_brush_color_p(comp, black, &s->fill))) return;
	WUC_Brush *fill_base = color_brush_base(s->fill);
	if (!fill_base) return;
	( void ) wuc_composition_sprite_shape_put__fill_brush(s->shape, fill_base);
	(( IUnknown * ) fill_base)->lpVtbl->Release(( IUnknown * ) fill_base);
}

static bool shape_build_visual(FluxShapeRect *s, WUC_Comp *comp, float w, float h) {
	if (FAILED(wuc_comp_create_shape_visual(comp, &s->visual))) return false;
	s->base = flux_compose_as_visual(s->visual);
	if (!s->base) return false;
	WFN_Vector_2 vsize = {w, h};
	( void ) wuc_visual_put__size(s->base, vsize);

	WUC_CompositionShapeCollection *shapes = NULL;
	if (FAILED(wuc_shape_visual_get__shapes(s->visual, &shapes))) return false;
	WUC_CompositionShape *shape_base = NULL;
	if (SUCCEEDED(cwinrt_query(s->shape, &CWINRT_IID_WUC_ICompositionShape, ( void ** ) &shape_base))) {
		( void ) wuc_composition_shape_collection_append(shapes, shape_base);
		(( IUnknown * ) shape_base)->lpVtbl->Release(( IUnknown * ) shape_base);
	}
	(( IUnknown * ) shapes)->lpVtbl->Release(( IUnknown * ) shapes);
	return true;
}

FluxShapeRect *flux_shape_rect_create(FluxCompositor *c, float w, float h, float radius) {
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxShapeRect *s = ( FluxShapeRect * ) calloc(1, sizeof(*s));
	if (!s) return NULL;
	s->c = comp;

	if (!shape_build_geometry(s, comp, w, h, radius)) goto fail;
	shape_set_fill(s, comp);
	if (!shape_build_visual(s, comp, w, h)) goto fail;
	return s;

fail:
	flux_shape_rect_destroy(s);
	return NULL;
}

void flux_shape_rect_destroy(FluxShapeRect *s) {
	if (!s) return;
	if (s->stroke) (( IUnknown * ) s->stroke)->lpVtbl->Release(( IUnknown * ) s->stroke);
	if (s->fill) (( IUnknown * ) s->fill)->lpVtbl->Release(( IUnknown * ) s->fill);
	if (s->shape) (( IUnknown * ) s->shape)->lpVtbl->Release(( IUnknown * ) s->shape);
	if (s->geometry) (( IUnknown * ) s->geometry)->lpVtbl->Release(( IUnknown * ) s->geometry);
	if (s->base) (( IUnknown * ) s->base)->lpVtbl->Release(( IUnknown * ) s->base);
	if (s->visual) (( IUnknown * ) s->visual)->lpVtbl->Release(( IUnknown * ) s->visual);
	free(s);
}

void flux_shape_setsize(FluxShapeRect *s, float w, float h) {
	if (!s) return;
	WFN_Vector_2 size = {w, h};
	if (s->geometry) ( void ) wuc_composition_rounded_rectangle_geometry_put__size(s->geometry, size);
	if (s->base) ( void ) wuc_visual_put__size(s->base, size);
}

void flux_shape_setradius(FluxShapeRect *s, float radius) {
	if (!s || !s->geometry) return;
	WFN_Vector_2 cr = {radius, radius};
	( void ) wuc_composition_rounded_rectangle_geometry_put__corner_radius(s->geometry, cr);
}

void flux_shape_setfillbrush(FluxShapeRect *s, WUC_Brush *brush) {
	if (!s || !s->shape || !brush) return;
	( void ) wuc_composition_sprite_shape_put__fill_brush(s->shape, brush);
}

WUC_Visual *flux_shape_rect_visual(FluxShapeRect *s) { return s ? s->base : NULL; }
