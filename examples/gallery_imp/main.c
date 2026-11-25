#include "app.h"
#include "gallery_assets.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	flux_controls_all();
	gallery_assets_init();
	GalleryApp g;
	if (!gallery_app_create(&g)) {
		gallery_app_destroy(&g);
		gallery_assets_clear();
		return 1;
	}
	int rc = gallery_app_run(&g);
	gallery_app_destroy(&g);
	gallery_assets_clear();
	return rc;
}
