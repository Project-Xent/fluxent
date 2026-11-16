#include "shell/flux_data_internal.h"

#include <stdlib.h>

bool flux_clipboard_set(FluxData const *data) {
	IDataObject *obj = flux_data_object(data);
	return obj && SUCCEEDED(OleSetClipboard(obj)) && SUCCEEDED(OleFlushClipboard());
}

FluxData *flux_clipboard_get(void) {
	IDataObject *obj = NULL;
	return SUCCEEDED(OleGetClipboard(&obj)) ? flux_data_adopt(obj) : NULL;
}

bool flux_clipboard_settext(char const *utf8, size_t len) {
	FluxData *data = flux_data_create();
	bool      ok   = data && flux_data_settext(data, utf8, len) && flux_clipboard_set(data);
	flux_data_destroy(data);
	return ok;
}

char *flux_clipboard_text(void) {
	FluxData *data = flux_clipboard_get();
	char     *text = data ? flux_data_text(data) : NULL;
	flux_data_destroy(data);
	return text;
}
