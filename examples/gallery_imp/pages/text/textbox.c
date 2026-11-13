#include "app.h"

#include <string.h>

static void page_textbox_sync(GalleryApp *g);

bool        page_textbox_build(PageBuild *b) {
	page_sync(b, page_textbox_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);

	page_header(b, parent, "TextBox", "A TextBox posts its content on every edit; the model decides what to keep.");
	{
		XtkNodeRef ex      = page_example(b, parent, "A simple TextBox.");
		PageDemo   demo    = page_demo(b, ex);
		XtkLocalId textbox = xtk_textbox_create(
		  &b->txn, demo.items,
		  &(XtkTextBoxProps) {
			.set = XTK_TEXTBOX_W_MASK | XTK_TEXTBOX_TEXT_MASK | XTK_TEXTBOX_ENABLED_MASK | XTK_TEXTBOX_PLACEHOLDER_MASK,
			.w   = 300.0f,
			.text        = m->name,
			.enabled     = true,
			.placeholder = "Enter your name",
		  }
		);
		xtk_textbox_onchange(&b->txn, xtk_ref_local(textbox), page_post, page_bind(b, MSG_NAME));
		page_keep(b, MSG_NAME, textbox);
		XtkLocalId output = page_output(
		  b, demo.row, m->name && m->name [0] ? gallery_fmt(b->app, "Hello, %s!", m->name) : "Waiting for input…"
		);
		page_keepout(b, MSG_NAME, output);
	}
	{
		XtkNodeRef ex       = page_example(b, parent, "A PasswordBox hides what you type.");
		PageDemo   demo     = page_demo(b, ex);
		XtkLocalId password = xtk_password_create(
		  &b->txn, demo.items,
		  &(XtkInputProps) {
			.set         = XTK_INPUT_W_MASK | XTK_INPUT_ENABLED_MASK | XTK_INPUT_PLACEHOLDER_MASK,
			.w           = 300.0f,
			.enabled     = true,
			.placeholder = "Enter a password",
		  }
		);
		xtk_password_onchange(&b->txn, xtk_ref_local(password), page_post, page_bind(b, MSG_PASSWORD));
		page_keep(b, MSG_PASSWORD, password);
		XtkLocalId output = page_output(
		  b, demo.row, gallery_fmt(b->app, "%d character%s", m->pass_chars, m->pass_chars == 1 ? "" : "s")
		);
		page_keepout(b, MSG_PASSWORD, output);
	}
	return true;
}

static void page_textbox_sync(GalleryApp *g) {
	char const *name
	  = g->model.name && g->model.name [0] ? gallery_fmt(g, "Hello, %s!", g->model.name) : "Waiting for input…";
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_NAME], name, ( uint32_t ) strlen(name), (XtkChangeStamp) {0}
	);
	char const *password = gallery_fmt(g, "%d character%s", g->model.pass_chars, g->model.pass_chars == 1 ? "" : "s");
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_PASSWORD], password, ( uint32_t ) strlen(password),
	  (XtkChangeStamp) {0}
	);
}
