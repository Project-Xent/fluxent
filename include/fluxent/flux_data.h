#ifndef FLUXENT_FLUX_DATA_H
#define FLUXENT_FLUX_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FluxData FluxData;

typedef struct FluxBitmap {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint8_t *pixels;
} FluxBitmap;

FluxData       *flux_data_create(void);
void            flux_data_destroy(FluxData *data);

/** Copies `utf8`. The caller may free it. */
bool            flux_data_settext(FluxData *data, char const *utf8, size_t len);
/** Copies `paths`. The caller may free it. */
bool            flux_data_setfiles(FluxData *data, char const *paths);
/** Copies `bitmap->pixels`. The caller may free them. */
bool            flux_data_setbitmap(FluxData *data, FluxBitmap const *bitmap);
/** Copies `bytes`. The caller may free them. */
bool            flux_data_setcustom(FluxData *data, char const *format, void const *bytes, size_t len);

/** malloc'd UTF-8. The caller frees it. */
char           *flux_data_text(FluxData const *data);
/** malloc'd UTF-8, one path per line. The caller frees it. */
char           *flux_data_files(FluxData const *data);
/** Copies pixels into `out`. The caller frees `out->pixels`. */
bool            flux_data_bitmap(FluxData const *data, FluxBitmap *out);
/** malloc'd bytes. The caller frees the pointer. `*len` is the byte count. */
void           *flux_data_custom(FluxData const *data, char const *format, size_t *len);

bool            flux_clipboard_set(FluxData const *data);
/** Owned. The caller destroys it. */
FluxData       *flux_clipboard_get(void);
/** Copies `utf8`. The caller may free it. */
bool            flux_clipboard_settext(char const *utf8, size_t len);
/** malloc'd UTF-8. The caller frees it. */
char           *flux_clipboard_text(void);

/** Borrowed, and only while a drag or drop callback is running. */
FluxData const *flux_drag_data(void);

#endif
