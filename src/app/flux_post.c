#include "app/flux_post.h"

#include <windows.h>

#include <stdlib.h>
#include <string.h>

#define FLUX_WM_POST (WM_APP + 0x60)

struct FluxPostSink {
	HWND   hwnd;
	XtkRt *rt;
};

typedef struct PostItem {
	XtkMsg   msg;
	uint32_t len;
	bool     text;
	char     data [];
} PostItem;

static wchar_t const kPostClass [] = L"FluxentPostSink";

static void          item_deliver(XtkRt *rt, PostItem *item) {
	if (item->text) ( void ) xtk_rt_poststring(rt, item->msg, item->data, item->len);
	else ( void ) xtk_rt_post(rt, item->msg);
	free(item);
}

static LRESULT CALLBACK sink_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg != FLUX_WM_POST) return DefWindowProcW(hwnd, msg, wp, lp);
	FluxPostSink *sink = ( FluxPostSink * ) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (sink) item_deliver(sink->rt, ( PostItem * ) lp);
	else free(( void * ) lp);
	return 0;
}

static bool register_class(void) {
	WNDCLASSEXW wc = {
	  .cbSize        = sizeof(wc),
	  .lpfnWndProc   = sink_proc,
	  .hInstance     = GetModuleHandleW(NULL),
	  .lpszClassName = kPostClass,
	};
	return RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

FluxPostSink *flux_postsink_create(XtkRt *rt) {
	if (!rt || !register_class()) return NULL;
	FluxPostSink *sink = ( FluxPostSink * ) calloc(1, sizeof(*sink));
	if (!sink) return NULL;
	sink->rt   = rt;
	sink->hwnd = CreateWindowExW(0, kPostClass, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
	if (!sink->hwnd) {
		free(sink);
		return NULL;
	}
	SetWindowLongPtrW(sink->hwnd, GWLP_USERDATA, ( LONG_PTR ) sink);
	return sink;
}

void flux_postsink_destroy(FluxPostSink *sink) {
	if (!sink) return;
	MSG pending;
	while (PeekMessageW(&pending, sink->hwnd, FLUX_WM_POST, FLUX_WM_POST, PM_REMOVE)) free(( void * ) pending.lParam);
	DestroyWindow(sink->hwnd);
	free(sink);
}

static PostItem *item_create(XtkMsg msg, char const *data, uint32_t len, bool text) {
	if (!text) len = 0;
	PostItem *item = ( PostItem * ) malloc(sizeof(*item) + len + 1u);
	if (!item) return NULL;
	item->msg  = msg;
	item->len  = len;
	item->text = text;
	if (len > 0u) memcpy(item->data, data, len);
	item->data [len] = '\0';
	return item;
}

XtkPostStatus flux_postsink_send(FluxPostSink *sink, XtkMsg msg, char const *data, uint32_t len, bool text) {
	if (!sink || msg.tag == 0 || (text && len > 0u && !data)) return XTK_POST_INVALID_MESSAGE;
	PostItem *item = item_create(msg, data, len, text);
	if (!item) return XTK_POST_OOM;
	if (PostMessageW(sink->hwnd, FLUX_WM_POST, 0, ( LPARAM ) item)) return XTK_POST_ACCEPTED;
	free(item);
	return XTK_POST_FULL;
}
