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

/** malloc'd UTF-8, one path per line. NULL if cancelled. The caller frees it. */
char         *flux_filedialog_show(FluxApp *app, FluxFileDialogDesc const *desc);

/** Copies the strings in `desc`. The caller may free them. The chosen paths are posted as one UTF-8 path per line. */
XtkPostStatus flux_filedialog_post(FluxApp *app, FluxFileDialogDesc const *desc, XtkMsg reply);

#endif
