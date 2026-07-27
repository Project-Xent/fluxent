/**
 * @file flux_init_textbox.h
 * @brief Private Desc + init prototypes for the text-input family:
 * TextBox / PasswordBox / NumberBox (+ the app-owned-context variants).
 */
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

/** @brief Create a text box node. */
XentNodeId flux_textbox_init(FluxTextBoxDesc const *info);

/** @brief Point absorbed TextBox / PasswordBox / NumberBox runtime at the live Host node. */
void       flux_textbox_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);

/** @brief Refresh the focused text control's native IME anchor after layout. */
void       flux_textbox_sync_ime(FluxStore *store, XentNodeId node);

/** @brief Create a text box node using the app-owned context and store. */
XentNodeId flux_app_create_textbox(FluxAppTextBoxDesc const *info);

/** @brief Create a password box node using the app-owned context and store. */
XentNodeId flux_app_create_password(FluxAppTextBoxDesc const *info);

/** @brief Create a number box node using the app-owned context and store. */
XentNodeId flux_app_create_number(FluxAppNumberBoxDesc const *info);

/** @brief Create a password box node. */
XentNodeId flux_password_init(FluxTextBoxDesc const *info);

/** @brief Create a number box node. */
XentNodeId flux_number_init(FluxNumberBoxDesc const *info);

#endif
