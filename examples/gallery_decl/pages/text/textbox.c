#include "gallery.h"

XtkEl *page_textbox(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "TextBox", "A TextBox posts its content on every edit; the model decides what to keep."),
		example(
		  ui, "A simple TextBox.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_textbox(ui, "Enter your name", (XtkTextBoxDesc) {.content = m->name, .change = xtk_msg(MSG_NAME)}),
			  300, NAN
			),
			m->name && m->name [0] ? xtk_fmt(ui, "Hello, %s!", m->name) : "Waiting for input…", NULL
		  )
		),
		example(
		  ui, "A PasswordBox hides what you type.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_password(
				ui, "Enter a password", (XtkTextBoxDesc) {.content = m->password, .change = xtk_msg(MSG_PASSWORD)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%d character%s", m->pass_chars, m->pass_chars == 1 ? "" : "s"), NULL
		  )
		)
	  )
	);
}
