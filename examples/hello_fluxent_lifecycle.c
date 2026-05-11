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
	( void ) d;
	return true;
}

bool demo_create_app(Demo *d) {
	FluxAppConfig cfg = {0};
	cfg.title         = L"Hello Fluxent - All Controls";
	cfg.width         = HELLO_FLUXENT_DEMO_WINDOW_W;
	cfg.height        = HELLO_FLUXENT_DEMO_WINDOW_H;
	cfg.dark_mode     = flux_theme_system_is_dark();
	cfg.backdrop      = FLUX_BACKDROP_MICA;

	d->app            = flux_app_create(&cfg);
	if (!d->app) return false;

	d->ctx         = flux_app_get_context(d->app);
	d->store       = flux_app_get_store(d->app);
	d->scroll_root = flux_app_get_root(d->app);
	if (!d->ctx || !d->store || d->scroll_root == XENT_NODE_INVALID) return false;
	return demo_alloc_state(d);
}

void demo_destroy(Demo *d) {
	if (d->menu) flux_menu_flyout_destroy(d->menu);
	if (d->flyout) flux_flyout_destroy(d->flyout);
	if (d->app) flux_app_destroy(d->app);
	free(d->counter);
	free(d->repeat);
	free(d->toggle);
	free(d->radio);
	free(d->members);
}
