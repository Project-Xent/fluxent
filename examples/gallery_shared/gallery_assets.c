#include "gallery_assets.h"

#include <objbase.h>
#include <stdio.h>

static unsigned char const logo [] = {
#include "logo.svg.h"
};
static unsigned char const spinner [] = {
#include "spinner.gif.h"
};
static unsigned char const bounce [] = {
#include "bounce.png.h"
};
static unsigned char const frame [] = {
#include "frame.png.h"
};

typedef struct GalleryAsset {
	wchar_t const       *name;
	unsigned char const *data;
	DWORD                size;
} GalleryAsset;

static GalleryAsset const assets [] = {
  {L"logo.svg",    logo,    sizeof(logo)   },
  {L"spinner.gif", spinner, sizeof(spinner)},
  {L"bounce.png",  bounce,  sizeof(bounce) },
  {L"frame.png",   frame,   sizeof(frame)  },
};

static wchar_t asset_dir [MAX_PATH];

static bool    asset_path(wchar_t output [MAX_PATH], wchar_t const *name) {
	int n = swprintf(output, MAX_PATH, L"%ls\\%ls", asset_dir, name);
	return n > 0 && n < MAX_PATH;
}

static bool asset_write(GalleryAsset const *asset) {
	wchar_t path [MAX_PATH];
	if (!asset_path(path, asset->name)) return false;
	HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	bool  ok      = WriteFile(file, asset->data, asset->size, &written, NULL) && written == asset->size;
	CloseHandle(file);
	return ok;
}

static bool asset_create_dir(void) {
	wchar_t temp [MAX_PATH];
	DWORD   length = GetTempPathW(MAX_PATH, temp);
	if (!length || length >= MAX_PATH) return false;
	GUID    id;
	wchar_t key [40];
	if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(&id, key, ARRAYSIZE(key))) return false;
	int n = swprintf(asset_dir, MAX_PATH, L"%lsFluxentGallery-%ls", temp, key);
	return n > 0 && n < MAX_PATH && CreateDirectoryW(asset_dir, NULL);
}

void gallery_assets_init(void) {
	if (asset_dir [0]) return;
	if (!asset_create_dir()) {
		asset_dir [0] = L'\0';
		return;
	}
	for (size_t i = 0; i < ARRAYSIZE(assets); i++) {
		if (asset_write(&assets [i])) continue;
		gallery_assets_clear();
		return;
	}
}

void gallery_assets_clear(void) {
	if (!asset_dir [0]) return;
	for (size_t i = 0; i < ARRAYSIZE(assets); i++) {
		wchar_t path [MAX_PATH];
		if (asset_path(path, assets [i].name)) DeleteFileW(path);
	}
	RemoveDirectoryW(asset_dir);
	asset_dir [0] = L'\0';
}

static XtkResource asset_resource(wchar_t const *path) {
	char utf8 [MAX_PATH * 4];
	if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), NULL, NULL)) return 0;
	return flux_image_respath(utf8);
}

XtkResource gallery_asset(wchar_t const *name) {
	wchar_t path [MAX_PATH];
	if (!asset_dir [0] || !asset_path(path, name)) return 0;
	return asset_resource(path);
}

XtkResource gallery_wallpaper(void) {
	wchar_t windows [MAX_PATH], path [MAX_PATH];
	UINT    length = GetWindowsDirectoryW(windows, ARRAYSIZE(windows));
	if (!length || length >= ARRAYSIZE(windows)) return gallery_asset(L"frame.png");
	int n = swprintf(path, ARRAYSIZE(path), L"%ls\\Web\\Wallpaper\\Windows\\img0.jpg", windows);
	if (n <= 0 || n >= MAX_PATH || GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
		return gallery_asset(L"frame.png");
	return asset_resource(path);
}
