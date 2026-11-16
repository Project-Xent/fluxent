#ifndef FLUXENT_FLUX_FILEDIALOG_H
#define FLUXENT_FLUX_FILEDIALOG_H

#include "flux_app.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum FluxFileDialogKind
{
	FLUX_FILEDIALOG_OPEN = 0,
	FLUX_FILEDIALOG_SAVE,
	FLUX_FILEDIALOG_FOLDER,
} FluxFileDialogKind;

typedef struct FluxFileFilter {
	char const *name;
	char const *spec;
} FluxFileFilter;

typedef struct FluxFileDialogDesc {
	FluxFileDialogKind    kind;
	char const           *title;
	char const           *folder;
	char const           *name;
	char const           *ext;
	FluxFileFilter const *filters;
	uint32_t              filter_count;
	bool                  multiple;
} FluxFileDialogDesc;

char         *flux_filedialog_show(FluxApp *app, FluxFileDialogDesc const *desc);

XtkPostStatus flux_filedialog_post(FluxApp *app, FluxFileDialogDesc const *desc, XtkMsg reply);

#endif
