#ifndef FLUXENT_FLUX_DATA_H
#define FLUXENT_FLUX_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A bundle of transfer formats (an OLE data object), shared by the clipboard and drag and drop. Text is UTF-8;
   file lists are newline-separated UTF-8 paths, as flux_filedialog returns them; bitmaps are 32-bit BGRA with
   straight alpha, top-down. Getters return malloc'd results, or NULL / false when the format is absent. A custom
   format is addressed by its registered clipboard format name. */
typedef struct FluxData FluxData;

typedef struct FluxBitmap {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint8_t *pixels;
} FluxBitmap;

FluxData *flux_data_create(void);
void      flux_data_destroy(FluxData *data);

bool      flux_data_settext(FluxData *data, char const *utf8, size_t len);
bool      flux_data_setfiles(FluxData *data, char const *paths);
bool      flux_data_setbitmap(FluxData *data, FluxBitmap const *bitmap);
bool      flux_data_setcustom(FluxData *data, char const *format, void const *bytes, size_t len);

char     *flux_data_text(FluxData const *data);
char     *flux_data_files(FluxData const *data);
bool      flux_data_bitmap(FluxData const *data, FluxBitmap *out);
void     *flux_data_custom(FluxData const *data, char const *format, size_t *len);

/* The clipboard keeps a rendered copy, so data may be destroyed right after and survives the app exiting. */
bool      flux_clipboard_set(FluxData const *data);
FluxData *flux_clipboard_get(void);
bool      flux_clipboard_settext(char const *utf8, size_t len);
char     *flux_clipboard_text(void);

#endif
