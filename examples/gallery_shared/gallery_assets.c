#include "gallery_assets.h"

#include <windows.h>

#include <stdio.h>
#include <wchar.h>

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

static wchar_t const kPrefix [] = L"FluxentGallery-";

static bool          temp_dir(wchar_t out [MAX_PATH]) {
	DWORD n = GetTempPathW(MAX_PATH, out);
	return n && n < MAX_PATH;
}

static bool process_dir(wchar_t out [MAX_PATH], DWORD pid) {
	wchar_t temp [MAX_PATH];
	if (!temp_dir(temp)) return false;
	int n = swprintf(out, MAX_PATH, L"%ls%ls%lu", temp, kPrefix, pid);
	return n > 0 && n < MAX_PATH;
}

static bool asset_path(wchar_t out [MAX_PATH], wchar_t const *dir, wchar_t const *name) {
	int n = swprintf(out, MAX_PATH, L"%ls\\%ls", dir, name);
	return n > 0 && n < MAX_PATH;
}

static void dir_remove(wchar_t const *dir) {
	for (size_t i = 0; i < ARRAYSIZE(assets); i++) {
		wchar_t path [MAX_PATH];
		if (asset_path(path, dir, assets [i].name)) DeleteFileW(path);
	}
	RemoveDirectoryW(dir);
}

static bool asset_write(wchar_t const *dir, GalleryAsset const *asset) {
	wchar_t path [MAX_PATH];
	if (!asset_path(path, dir, asset->name)) return false;
	HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS;
	DWORD written = 0;
	bool  ok      = WriteFile(file, asset->data, asset->size, &written, NULL) && written == asset->size;
	CloseHandle(file);
	return ok;
}

static bool process_alive(DWORD pid) {
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
	DWORD code  = 0;
	bool  alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
	CloseHandle(process);
	return alive;
}

static bool stale_dir(WIN32_FIND_DATAW const *found) {
	if (!(found->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
	wchar_t const *digits = found->cFileName + ARRAYSIZE(kPrefix) - 1;
	wchar_t       *end    = NULL;
	unsigned long  pid    = wcstoul(digits, &end, 10);
	if (end == digits || *end || pid == GetCurrentProcessId()) return false;
	return !process_alive(( DWORD ) pid);
}

static void sweep_stale(void) {
	wchar_t temp [MAX_PATH], pattern [MAX_PATH];
	if (!temp_dir(temp) || swprintf(pattern, MAX_PATH, L"%ls%ls*", temp, kPrefix) <= 0) return;
	WIN32_FIND_DATAW found;
	HANDLE           find = FindFirstFileW(pattern, &found);
	if (find == INVALID_HANDLE_VALUE) return;
	do {
		wchar_t dir [MAX_PATH];
		if (stale_dir(&found) && swprintf(dir, MAX_PATH, L"%ls%ls", temp, found.cFileName) > 0) dir_remove(dir);
	}
	while (FindNextFileW(find, &found));
	FindClose(find);
}

void gallery_assets_init(void) {
	sweep_stale();
	wchar_t dir [MAX_PATH];
	if (!process_dir(dir, GetCurrentProcessId())) return;
	if (!CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return;
	for (size_t i = 0; i < ARRAYSIZE(assets); i++) {
		if (asset_write(dir, &assets [i])) continue;
		dir_remove(dir);
		return;
	}
}

void gallery_assets_clear(void) {
	wchar_t dir [MAX_PATH];
	if (process_dir(dir, GetCurrentProcessId())) dir_remove(dir);
}

static XtkResource asset_resource(wchar_t const *path) {
	char utf8 [MAX_PATH * 4];
	if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), NULL, NULL)) return 0;
	return flux_image_respath(utf8);
}

XtkResource gallery_asset(wchar_t const *name) {
	wchar_t dir [MAX_PATH], path [MAX_PATH];
	if (!process_dir(dir, GetCurrentProcessId()) || !asset_path(path, dir, name)) return 0;
	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return 0;
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
