#include "app.h"

static XtkNodeRef section(PageBuild *b, XtkNodeRef parent, char const *title, char const *body) {
	XtkNodeRef card = xtk_ref_local(ui_card(&b->txn, parent, 4.0f, XENT_FLEX_ALIGN_START, 16.0f));
	ui_text(&b->txn, card, title, 16.0f, FLUX_FONT_SEMI_BOLD);
	page_note(b, card, body);
	return card;
}

bool page_settings_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "Settings", "About this gallery and its environment.");
	section(
	  b, parent, "App theme",
	  gallery_fmt(
		b->app, "Following the system theme: %s (set at startup).", flux_theme_systemdark() ? "Dark" : "Light"
	  )
	);
	XtkNodeRef about
	  = section(b, parent, "About", "FluXent Gallery — fluxent 0.3.1, imperative transactions over retained controls.");
	xtk_link_create(
	  &b->txn, about,
	  &(XtkLinkProps) {
		.set  = XTK_LINK_TEXT_MASK | XTK_LINK_URL_MASK,
		.text = "Source code",
		.url  = "https://github.com/Project-Xent",
	  }
	);
	return true;
}
