#include "wizard.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	Model model;
	wizard_init(&model);

	FluxAppCfg cfg = {
	  .title     = L"Xent Sample Setup",
	  .width     = 820,
	  .height    = 600,
	  .dark_mode = flux_theme_systemdark(),
	  .backdrop  = FLUX_BACKDROP_MICA,
	  .on_start  = wizard_start,
	  .on_stop   = wizard_stop,
	  .user      = &model,
	};
	int result = flux_run(&cfg, &model, wizard_update, wizard_view);
	wizard_free(&model);
	return result;
}
