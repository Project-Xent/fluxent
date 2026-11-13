#ifndef FLUX_INIT_PICKER_H
#define FLUX_INIT_PICKER_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>

typedef struct FluxDatePickerDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxInput  *input;
	FluxWindow *window;
	XentNodeId  overlay;
	char const *header;
	int         date;
	int         min_year;
	int         max_year;
	void        (*on_change)(void *ctx, int date);
	void       *userdata;
} FluxDatePickerDesc;

typedef struct FluxTimePickerDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxInput  *input;
	FluxWindow *window;
	XentNodeId  overlay;
	char const *header;
	int         minutes;
	int         clock;
	int         step;
	void        (*on_change)(void *ctx, int minutes);
	void       *userdata;
} FluxTimePickerDesc;

XentNodeId  flux_datepicker_init(FluxDatePickerDesc const *info);
XentNodeId  flux_timepicker_init(FluxTimePickerDesc const *info);

void        flux_picker_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id);

void        flux_picker_setheader(FluxStore *store, XentNodeId id, char const *header);
void        flux_picker_setenabled(FluxStore *store, XentNodeId id, bool enabled);

void        flux_datepicker_setdate(FluxStore *store, XentNodeId id, int date);
void        flux_datepicker_setminyear(FluxStore *store, XentNodeId id, int year);
void        flux_datepicker_setmaxyear(FluxStore *store, XentNodeId id, int year);

void        flux_timepicker_settime(FluxStore *store, XentNodeId id, int minutes);
void        flux_timepicker_setclock(FluxStore *store, XentNodeId id, int clock);
void        flux_timepicker_setstep(FluxStore *store, XentNodeId id, int step);

char const *flux_picker_valuetext(FluxStore *store, XentNodeId id);

struct FluxNodeData;
void flux_picker_sync(XentNodeId node, struct FluxNodeData *nd);

void flux_picker_close(FluxStore *store, XentNodeId id);

#endif
