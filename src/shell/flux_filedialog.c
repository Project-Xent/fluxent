#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_filedialog.h"
#include "runtime/flux_str.h"

#include <shobjidl.h>
#include <stdlib.h>
#include <string.h>

typedef struct PathList {
	char  *data;
	size_t len;
	size_t cap;
} PathList;

static wchar_t *wide_dup(char const *utf8) {
	if (!utf8) return NULL;
	int      n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t *w = n > 0 ? ( wchar_t * ) malloc(sizeof(wchar_t) * ( size_t ) n) : NULL;
	if (w) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
	return w;
}

static bool paths_append(PathList *list, wchar_t const *path) {
	int    n    = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
	size_t need = list->len + ( size_t ) n + 1u;
	if (n <= 0) return false;
	if (need > list->cap) {
		size_t cap  = need * 2u;
		char  *data = ( char * ) realloc(list->data, cap);
		if (!data) return false;
		list->data = data;
		list->cap  = cap;
	}
	if (list->len) list->data [list->len++] = '\n';
	WideCharToMultiByte(CP_UTF8, 0, path, -1, list->data + list->len, n, NULL, NULL);
	list->len += ( size_t ) n - 1u;
	return true;
}

static void paths_add_item(PathList *list, IShellItem *item) {
	wchar_t *path = NULL;
	if (FAILED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path))) return;
	( void ) paths_append(list, path);
	CoTaskMemFree(path);
}

typedef HRESULT (*DialogText)(IFileDialog *dlg, LPCWSTR text);

static HRESULT  dialog_title(IFileDialog *dlg, LPCWSTR text) { return IFileDialog_SetTitle(dlg, text); }

static HRESULT  dialog_name(IFileDialog *dlg, LPCWSTR text) { return IFileDialog_SetFileName(dlg, text); }

static HRESULT  dialog_ext(IFileDialog *dlg, LPCWSTR text) { return IFileDialog_SetDefaultExtension(dlg, text); }

static void     set_text(IFileDialog *dlg, char const *utf8, DialogText apply) {
	wchar_t *w = wide_dup(utf8);
	if (w) ( void ) apply(dlg, w);
	free(w);
}

static void set_folder(IFileDialog *dlg, char const *utf8) {
	wchar_t    *w    = wide_dup(utf8);
	IShellItem *item = NULL;
	if (w && SUCCEEDED(SHCreateItemFromParsingName(w, NULL, &IID_IShellItem, ( void ** ) &item))) {
		( void ) IFileDialog_SetFolder(dlg, item);
		IShellItem_Release(item);
	}
	free(w);
}

static void set_filters(IFileDialog *dlg, FluxFileFilter const *filters, uint32_t count) {
	COMDLG_FILTERSPEC *specs = count ? ( COMDLG_FILTERSPEC * ) calloc(count, sizeof(*specs)) : NULL;
	if (!specs) return;
	for (uint32_t i = 0; i < count; i++)
		specs [i] = (COMDLG_FILTERSPEC) {wide_dup(filters [i].name), wide_dup(filters [i].spec)};
	( void ) IFileDialog_SetFileTypes(dlg, count, specs);
	for (uint32_t i = 0; i < count; i++) {
		free(( void * ) specs [i].pszName);
		free(( void * ) specs [i].pszSpec);
	}
	free(specs);
}

static void configure(IFileDialog *dlg, FluxFileDialogDesc const *desc) {
	FILEOPENDIALOGOPTIONS opts = 0;
	( void ) IFileDialog_GetOptions(dlg, &opts);
	opts |= FOS_FORCEFILESYSTEM;
	if (desc->kind == FLUX_FILEDIALOG_FOLDER) opts |= FOS_PICKFOLDERS;
	if (desc->kind == FLUX_FILEDIALOG_OPEN && desc->multiple) opts |= FOS_ALLOWMULTISELECT;
	( void ) IFileDialog_SetOptions(dlg, opts);
	set_text(dlg, desc->title, dialog_title);
	set_text(dlg, desc->name, dialog_name);
	set_text(dlg, desc->ext, dialog_ext);
	set_folder(dlg, desc->folder);
	if (desc->kind != FLUX_FILEDIALOG_FOLDER) set_filters(dlg, desc->filters, desc->filter_count);
}

static void collect_many(IFileDialog *dlg, PathList *list) {
	IFileOpenDialog *open  = NULL;
	IShellItemArray *items = NULL;
	DWORD            count = 0;
	if (FAILED(IFileDialog_QueryInterface(dlg, &IID_IFileOpenDialog, ( void ** ) &open))) return;
	if (SUCCEEDED(IFileOpenDialog_GetResults(open, &items))) ( void ) IShellItemArray_GetCount(items, &count);
	for (DWORD i = 0; i < count; i++) {
		IShellItem *item = NULL;
		if (FAILED(IShellItemArray_GetItemAt(items, i, &item))) continue;
		paths_add_item(list, item);
		IShellItem_Release(item);
	}
	if (items) IShellItemArray_Release(items);
	IFileOpenDialog_Release(open);
}

static void collect_one(IFileDialog *dlg, PathList *list) {
	IShellItem *item = NULL;
	if (FAILED(IFileDialog_GetResult(dlg, &item))) return;
	paths_add_item(list, item);
	IShellItem_Release(item);
}

char *flux_filedialog_show(FluxApp *app, FluxFileDialogDesc const *desc) {
	if (!app || !desc) return NULL;
	IFileDialog *dlg   = NULL;
	CLSID const *clsid = desc->kind == FLUX_FILEDIALOG_SAVE ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog;
	if (FAILED(CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, ( void ** ) &dlg))) return NULL;
	configure(dlg, desc);
	PathList list = {0};
	if (SUCCEEDED(IFileDialog_Show(dlg, flux_app_hwnd(app)))) {
		if (desc->kind == FLUX_FILEDIALOG_OPEN && desc->multiple) collect_many(dlg, &list);
		else collect_one(dlg, &list);
	}
	IFileDialog_Release(dlg);
	return list.data;
}

typedef struct DialogJob {
	FluxApp           *app;
	XtkMsg             reply;
	FluxFileDialogDesc desc;
	FluxFileFilter    *filters;
} DialogJob;

static void job_free(DialogJob *job) {
	free(( void * ) job->desc.title);
	free(( void * ) job->desc.folder);
	free(( void * ) job->desc.name);
	free(( void * ) job->desc.ext);
	for (uint32_t i = 0; job->filters && i < job->desc.filter_count; i++) {
		free(( void * ) job->filters [i].name);
		free(( void * ) job->filters [i].spec);
	}
	free(job->filters);
	free(job);
}

static bool job_copy_filters(DialogJob *job, FluxFileFilter const *filters, uint32_t count) {
	job->desc.filters      = NULL;
	job->desc.filter_count = 0;
	if (!count) return true;
	job->filters = ( FluxFileFilter * ) calloc(count, sizeof(*job->filters));
	if (!job->filters) return false;
	for (uint32_t i = 0; i < count; i++)
		job->filters [i] = (FluxFileFilter) {flux_str_dup(filters [i].name), flux_str_dup(filters [i].spec)};
	job->desc.filters      = job->filters;
	job->desc.filter_count = count;
	return true;
}

static DialogJob *job_copy(FluxApp *app, FluxFileDialogDesc const *desc, XtkMsg reply) {
	DialogJob *job = ( DialogJob * ) calloc(1, sizeof(*job));
	if (!job) return NULL;
	job->app         = app;
	job->reply       = reply;
	job->desc        = *desc;
	job->desc.title  = flux_str_dup(desc->title);
	job->desc.folder = flux_str_dup(desc->folder);
	job->desc.name   = flux_str_dup(desc->name);
	job->desc.ext    = flux_str_dup(desc->ext);
	if (job_copy_filters(job, desc->filters, desc->filter_count)) return job;
	job_free(job);
	return NULL;
}

static void job_run(void *ctx) {
	DialogJob *job   = ( DialogJob * ) ctx;
	char      *paths = flux_filedialog_show(job->app, &job->desc);
	XtkMsg     reply = job->reply;
	reply.b          = paths != NULL;
	( void ) flux_app_poststring(job->app, reply, paths ? paths : "", paths ? ( uint32_t ) strlen(paths) : 0u);
	free(paths);
	job_free(job);
}

XtkPostStatus flux_filedialog_post(FluxApp *app, FluxFileDialogDesc const *desc, XtkMsg reply) {
	if (!app || !desc || reply.tag == 0) return XTK_POST_INVALID_MESSAGE;
	DialogJob *job = job_copy(app, desc, reply);
	if (!job) return XTK_POST_OOM;
	if (flux_app_invoke(app, job_run, job)) return XTK_POST_ACCEPTED;
	job_free(job);
	return XTK_POST_FULL;
}
