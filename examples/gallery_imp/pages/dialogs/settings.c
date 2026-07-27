#include "app.h"
#include <math.h>
#include <stdio.h>
#include "host/flux_prop_apply.h"
#include "render/flux_icon.h"

void page_settings(GalleryApp *g, XentNodeId parent) {
	bool dark = flux_theme_systemdark();
	g_page_header(g, parent, "Settings", "About this gallery and its environment.");
	{
		XentNodeId card = g_card(g, parent);
		g_text(g, card, "App theme", 16.0f, FLUX_FONT_SEMI_BOLD);
		g_text(
		  g, card, gallery_fmt(g, "Following the system theme: %s (set at startup).", dark ? "Dark" : "Light"), 13.0f,
		  FLUX_FONT_REGULAR
		);
	}
	{
		XentNodeId card = g_card(g, parent);
		g_text(g, card, "About", 16.0f, FLUX_FONT_SEMI_BOLD);
		g_text(
		  g, card, "FluXent Gallery — fluxent 0.3.0, declarative layer over retained controls.", 13.0f,
		  FLUX_FONT_REGULAR
		);
		flux_link_init(&(FluxHyperlinkDesc) {
		  .ctx    = gallery_ctx(g),
		  .store  = gallery_store(g),
		  .parent = card,
		  .label  = "Source code",
		  .url    = "https://github.com/Project-Xent",
		});
	}
}
