#include "shell/flux_data_internal.h"

#include <shellapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

struct FluxData {
	IDataObject *obj;
};

FluxData *flux_data_adopt(IDataObject *obj) {
	FluxData *data = obj ? ( FluxData * ) malloc(sizeof(*data)) : NULL;
	if (data) data->obj = obj;
	else if (obj) IDataObject_Release(obj);
	return data;
}

IDataObject *flux_data_object(FluxData const *data) { return data ? data->obj : NULL; }

FluxData    *flux_data_create(void) {
	IDataObject *obj = NULL;
	if (FAILED(SHCreateDataObject(NULL, 0, NULL, NULL, &IID_IDataObject, ( void ** ) &obj))) return NULL;
	return flux_data_adopt(obj);
}

void flux_data_destroy(FluxData *data) {
	if (!data) return;
	IDataObject_Release(data->obj);
	free(data);
}

static FORMATETC format_of(CLIPFORMAT cf) { return (FORMATETC) {cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}; }

bool             flux_data_put(FluxData *data, CLIPFORMAT cf, HGLOBAL h) {
	if (!data || !h) {
		if (h) GlobalFree(h);
		return false;
	}
	FORMATETC fe = format_of(cf);
	STGMEDIUM sm = {.tymed = TYMED_HGLOBAL, .hGlobal = h};
	if (SUCCEEDED(IDataObject_SetData(data->obj, &fe, &sm, TRUE))) return true;
	GlobalFree(h);
	return false;
}

bool flux_data_take(FluxData const *data, CLIPFORMAT cf, STGMEDIUM *sm) {
	FORMATETC fe = format_of(cf);
	if (!data || FAILED(IDataObject_GetData(data->obj, &fe, sm))) return false;
	if (sm->tymed == TYMED_HGLOBAL && sm->hGlobal) return true;
	ReleaseStgMedium(sm);
	return false;
}

static HGLOBAL global_bytes(void const *bytes, size_t len) {
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len ? len : 1u);
	void   *p = h ? GlobalLock(h) : NULL;
	if (!p) {
		if (h) GlobalFree(h);
		return NULL;
	}
	if (len) memcpy(p, bytes, len);
	GlobalUnlock(h);
	return h;
}

static CLIPFORMAT custom_format(char const *name) {
	wchar_t wide [256];
	if (!name || MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, 256) <= 0) return 0;
	return ( CLIPFORMAT ) RegisterClipboardFormatW(wide);
}

bool flux_data_setcustom(FluxData *data, char const *format, void const *bytes, size_t len) {
	CLIPFORMAT cf = custom_format(format);
	return cf && (bytes || !len) && flux_data_put(data, cf, global_bytes(bytes, len));
}

static void *global_copy(HGLOBAL h, size_t *len) {
	size_t n   = GlobalSize(h);
	void  *src = GlobalLock(h);
	if (!src) return NULL;
	void *out = malloc(n + 1u);
	if (out) memcpy(out, src, n);
	if (out && len) *len = n;
	GlobalUnlock(h);
	return out;
}

void *flux_data_custom(FluxData const *data, char const *format, size_t *len) {
	CLIPFORMAT cf = custom_format(format);
	STGMEDIUM  sm = {0};
	if (!cf || !flux_data_take(data, cf, &sm)) return NULL;
	void *out = global_copy(sm.hGlobal, len);
	ReleaseStgMedium(&sm);
	return out;
}

static HGLOBAL global_wide(char const *utf8, int len, size_t header) {
	int     n = len ? MultiByteToWideChar(CP_UTF8, 0, utf8, len, NULL, 0) : 0;
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, header + (( size_t ) n + 2u) * sizeof(wchar_t));
	char   *p = h ? ( char * ) GlobalLock(h) : NULL;
	if (!p) {
		if (h) GlobalFree(h);
		return NULL;
	}
	if (n) MultiByteToWideChar(CP_UTF8, 0, utf8, len, ( wchar_t * ) (p + header), n);
	GlobalUnlock(h);
	return h;
}

static char *utf8_of(wchar_t const *wide, int len) {
	int   n   = WideCharToMultiByte(CP_UTF8, 0, wide, len, NULL, 0, NULL, NULL);
	char *out = ( char * ) malloc(( size_t ) (n > 0 ? n : 0) + 1u);
	if (!out) return NULL;
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, wide, len, out, n, NULL, NULL);
	out [n > 0 ? n : 0] = '\0';
	return out;
}

bool flux_data_settext(FluxData *data, char const *utf8, size_t len) {
	return utf8 && len <= INT32_MAX && flux_data_put(data, CF_UNICODETEXT, global_wide(utf8, ( int ) len, 0u));
}

char *flux_data_text(FluxData const *data) {
	STGMEDIUM sm = {0};
	if (!flux_data_take(data, CF_UNICODETEXT, &sm)) return NULL;
	wchar_t const *wide = ( wchar_t const * ) GlobalLock(sm.hGlobal);
	size_t         cap  = GlobalSize(sm.hGlobal) / sizeof(wchar_t);
	char          *out  = wide ? utf8_of(wide, ( int ) wcsnlen(wide, cap)) : NULL;
	if (wide) GlobalUnlock(sm.hGlobal);
	ReleaseStgMedium(&sm);
	return out;
}

static void split_lines(wchar_t *w) {
	for (; *w; w++)
		if (*w == L'\n') *w = L'\0';
}

static HGLOBAL drop_global(char const *paths, size_t len) {
	HGLOBAL h = global_wide(paths, ( int ) len, sizeof(DROPFILES));
	char   *p = h ? ( char * ) GlobalLock(h) : NULL;
	if (!p) {
		if (h) GlobalFree(h);
		return NULL;
	}
	*( DROPFILES * ) p = (DROPFILES) {.pFiles = sizeof(DROPFILES), .fWide = TRUE};
	split_lines(( wchar_t * ) (p + sizeof(DROPFILES)));
	GlobalUnlock(h);
	return h;
}

bool flux_data_setfiles(FluxData *data, char const *paths) {
	size_t len = paths ? strlen(paths) : 0u;
	return len && len <= INT32_MAX && flux_data_put(data, CF_HDROP, drop_global(paths, len));
}

static wchar_t *drop_join(HDROP drop, size_t *used) {
	UINT     count = DragQueryFileW(drop, 0xffffffffu, NULL, 0);
	wchar_t *list  = NULL;
	for (UINT i = 0; i < count; i++) {
		UINT     n    = DragQueryFileW(drop, i, NULL, 0);
		wchar_t *grow = ( wchar_t * ) realloc(list, (*used + n + 2u) * sizeof(wchar_t));
		if (!grow) break;
		list = grow;
		if (*used) list [(*used)++] = L'\n';
		*used += DragQueryFileW(drop, i, list + *used, n + 1u);
	}
	return list;
}

char *flux_data_files(FluxData const *data) {
	STGMEDIUM sm   = {0};
	size_t    used = 0;
	if (!flux_data_take(data, CF_HDROP, &sm)) return NULL;
	wchar_t *list = drop_join(( HDROP ) sm.hGlobal, &used);
	ReleaseStgMedium(&sm);
	char *out = list ? utf8_of(list, ( int ) used) : NULL;
	free(list);
	return out;
}
