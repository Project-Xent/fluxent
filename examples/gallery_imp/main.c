#include "app.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	GalleryApp g;
	if (!gallery_app_create(&g)) {
		gallery_app_destroy(&g);
		return 1;
	}
	int rc = gallery_app_run(&g);
	gallery_app_destroy(&g);
	return rc;
}
