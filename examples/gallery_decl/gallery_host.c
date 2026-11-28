#include "gallery.h"
#include "gallery_assets.h"

#include <fluxent/flux_hot.h>

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	Model model;
	gallery_model_init(&model);
	model.backdrop = gallery_backdrop_load();

	FluxAppCfg cfg = {
	  .title    = L"FluXent Gallery (Declarative)",
	  .width    = 1100,
	  .height   = 760,
	  .backdrop = ( FluxBackdropType ) model.backdrop,
	};
	FluxHotCfg hot = {
	  .module    = L"fluxent_gallery_decl_view.dll",
	  .watch     = GALLERY_ROOT L"/examples",
	  .build_dir = GALLERY_ROOT,
	  .build     = L"xmake build -y fluxent_gallery_decl_view",
	};
	gallery_assets_init();
	int rc = flux_run_hot(&cfg, &hot, &model);
	gallery_assets_clear();
	return rc;
}
