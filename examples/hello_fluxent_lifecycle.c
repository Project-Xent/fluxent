#include "hello_fluxent_demo.h"
#include <stdio.h>
#include <stdlib.h>

static bool demo_alloc_state(Demo *d) {
	d->counter = ( CounterState * ) calloc(1, sizeof(*d->counter));
	d->repeat  = ( RepeatCounterState * ) calloc(1, sizeof(*d->repeat));
	d->toggle  = ( ToggleState * ) calloc(1, sizeof(*d->toggle));
	d->radio   = ( RadioGroup * ) calloc(1, sizeof(*d->radio));
	d->members = ( RadioGroupMember * ) calloc(RADIO_GROUP_MAX, sizeof(*d->members));
	if (!d->counter || !d->repeat || !d->toggle || !d->radio || !d->members) return false;

	d->counter->store = d->store;
	snprintf(d->counter->buf, sizeof(d->counter->buf), "Clicked: 0");
	d->repeat->store = d->store;
	snprintf(d->repeat->buf, sizeof(d->repeat->buf), "Repeated: 0");
	d->toggle->store = d->store;
	d->toggle->ctx   = d->ctx;
	d->radio->store  = d->store;
	return true;
}

bool demo_init(Demo *d) {
	XentConfig xcfg       = {0};
	xcfg.initial_capacity = 512;
	d->ctx                = xent_create_context(&xcfg);
	if (!d->ctx) return false;

	d->store = flux_node_store_create(512);
	if (!d->store) return false;
	return demo_alloc_state(d);
}

bool demo_create_app(Demo *d) {
	FluxAppConfig cfg = {0};
	cfg.title         = L"Hello Fluxent - All Controls";
	cfg.width         = 640;
	cfg.height        = 760;
	cfg.dark_mode     = flux_theme_system_is_dark();
	cfg.backdrop      = FLUX_BACKDROP_MICA;

	HRESULT hr        = flux_app_create(&cfg, &d->app);
	if (FAILED(hr)) return false;
	flux_app_set_root(d->app, d->ctx, d->scroll_root, d->store);

	if (!d->scroll_nd || !d->scroll_nd->component_data) return true;
	FluxScrollData *sd = ( FluxScrollData * ) d->scroll_nd->component_data;
	sd->content_h      = 2200.0f;
	sd->content_w      = 640.0f;
	return true;
}

void demo_destroy(Demo *d) {
	if (d->menu) flux_menu_flyout_destroy(d->menu);
	if (d->flyout) flux_flyout_destroy(d->flyout);
	if (d->app) flux_app_destroy(d->app);
	if (d->store) flux_node_store_destroy(d->store);
	if (d->ctx) xent_destroy_context(d->ctx);
	free(d->counter);
	free(d->repeat);
	free(d->toggle);
	free(d->radio);
	free(d->members);
}
