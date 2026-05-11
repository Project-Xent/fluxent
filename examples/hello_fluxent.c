#include "hello_fluxent_demo.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	Demo demo = {0};
	if (!demo_init(&demo)) {
		demo_destroy(&demo);
		return 1;
	}

	if (!demo_create_app(&demo)) {
		demo_destroy(&demo);
		return 1;
	}

	demo_build_static_content(&demo);
	demo_add_dynamic_content(&demo);
	int result = flux_app_run(demo.app);
	demo_destroy(&demo);
	return result;
}
