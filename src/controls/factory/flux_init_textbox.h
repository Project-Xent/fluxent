#ifndef FLUX_INIT_TEXTBOX_H
#define FLUX_INIT_TEXTBOX_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxTextBoxDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *placeholder;
	void        (*on_change)(void *, char const *);
	void       *userdata;
} FluxTextBoxDesc;

typedef struct FluxAppTextBoxDesc {
	FluxApp    *app;
	XentNodeId  parent;
	char const *placeholder;
	void        (*on_change)(void *, char const *);
	void       *userdata;
} FluxAppTextBoxDesc;

typedef struct FluxNumberBoxDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	double     min_value;
	double     max_value;
	double     step;
	void       (*on_value_change)(void *, double);
	void      *userdata;
} FluxNumberBoxDesc;

typedef struct FluxAppNumberBoxDesc {
	FluxApp   *app;
	XentNodeId parent;
	double     min_value;
	double     max_value;
	double     step;
	void       (*on_value_change)(void *, double);
	void      *userdata;
} FluxAppNumberBoxDesc;

XentNodeId flux_textbox_init(FluxTextBoxDesc const *info);

void       flux_textbox_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);

void       flux_textbox_sync_ime(FluxStore *store, XentNodeId node);

float      flux_textbox_wheel(FluxStore *store, XentNodeId node, float delta);

XentNodeId flux_app_create_textbox(FluxAppTextBoxDesc const *info);

XentNodeId flux_app_create_password(FluxAppTextBoxDesc const *info);

XentNodeId flux_app_create_number(FluxAppNumberBoxDesc const *info);

XentNodeId flux_password_init(FluxTextBoxDesc const *info);

XentNodeId flux_number_init(FluxNumberBoxDesc const *info);

#endif
