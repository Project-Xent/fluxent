#include "app/flux_post.h"

#include <windows.h>

#include <stdlib.h>
#include <string.h>

#define FLUX_WM_POST (WM_APP + 0x60)

struct FluxPostSink {
	HWND hwnd;
};

typedef struct PostItem {
	void  (*fn)(void *ctx);
	void *ctx;
	bool  owned;
} PostItem;

typedef struct MsgItem {
	XtkRt   *rt;
	XtkMsg   msg;
	uint32_t len;
	bool     text;
	char     data [];
} MsgItem;

static wchar_t const    kPostClass [] = L"FluxentPostSink";

static LRESULT CALLBACK sink_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg != FLUX_WM_POST) return DefWindowProcW(hwnd, msg, wp, lp);
	PostItem *item = ( PostItem * ) lp;
	item->fn(item->ctx);
	free(item);
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

FluxPostSink *flux_postsink_create(void) {
	if (!register_class()) return NULL;
	FluxPostSink *sink = ( FluxPostSink * ) calloc(1, sizeof(*sink));
	if (!sink) return NULL;
	sink->hwnd = CreateWindowExW(0, kPostClass, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
	if (sink->hwnd) return sink;
	free(sink);
	return NULL;
}

void flux_postsink_destroy(FluxPostSink *sink) {
	if (!sink) return;
	MSG pending;
	while (PeekMessageW(&pending, sink->hwnd, FLUX_WM_POST, FLUX_WM_POST, PM_REMOVE)) {
		PostItem *item = ( PostItem * ) pending.lParam;
		if (item->owned) free(item->ctx);
		free(item);
	}
	DestroyWindow(sink->hwnd);
	free(sink);
}

static bool sink_post(FluxPostSink *sink, void (*fn)(void *), void *ctx, bool owned) {
	PostItem *item = sink && fn ? ( PostItem * ) malloc(sizeof(*item)) : NULL;
	if (!item) return false;
	*item = (PostItem) {fn, ctx, owned};
	if (PostMessageW(sink->hwnd, FLUX_WM_POST, 0, ( LPARAM ) item)) return true;
	free(item);
	return false;
}

bool flux_postsink_invoke(FluxPostSink *sink, void (*fn)(void *ctx), void *ctx) {
	return sink_post(sink, fn, ctx, false);
}

static void msg_deliver(void *ctx) {
	MsgItem *item = ( MsgItem * ) ctx;
	if (item->text) ( void ) xtk_rt_poststring(item->rt, item->msg, item->data, item->len);
	else ( void ) xtk_rt_post(item->rt, item->msg);
	free(item);
}

XtkPostStatus flux_postsink_send(FluxPostSink *sink, XtkRt *rt, XtkMsg msg, char const *data, uint32_t len, bool text) {
	if (!sink || !rt || msg.tag == 0 || (text && len > 0u && !data)) return XTK_POST_INVALID_MESSAGE;
	if (!text) len = 0;
	MsgItem *item = ( MsgItem * ) malloc(sizeof(*item) + len + 1u);
	if (!item) return XTK_POST_OOM;
	*item = (MsgItem) {.rt = rt, .msg = msg, .len = len, .text = text};
	if (len > 0u) memcpy(item->data, data, len);
	item->data [len] = '\0';
	if (sink_post(sink, msg_deliver, item, true)) return XTK_POST_ACCEPTED;
	free(item);
	return XTK_POST_FULL;
}
