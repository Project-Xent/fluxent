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

/* Every string is UTF-8 and optional. spec is a ';'-separated pattern list ("*.png;*.jpg"). folder is where the
   dialog opens; name is the initial file name and ext the extension added when the user types none (save only);
   multiple applies to open only. */
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

/* Shows the shell file dialog modally over the app window. Returns the chosen paths as one malloc'd UTF-8 string,
   newline-separated, or NULL when the user cancels or the dialog fails. It runs a nested message loop, so never
   call it from an update or event callback; use flux_filedialog_post there. */
char         *flux_filedialog_show(FluxApp *app, FluxFileDialogDesc const *desc);

/* Copies desc, shows the dialog once the current message is done, then posts reply with b = true and the paths
   (formatted as flux_filedialog_show returns them) as its string, or b = false and an empty string on cancel. */
XtkPostStatus flux_filedialog_post(FluxApp *app, FluxFileDialogDesc const *desc, XtkMsg reply);

#endif
