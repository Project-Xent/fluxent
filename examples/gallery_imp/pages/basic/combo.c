#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"
#include "render/flux_icon.h"
#include "controls/behavior/flux_flyout_binding.h"

static void on_combo(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static char const *const kComboColors [] = {"Blue", "Green", "Red", "Yellow"};

static char const *const kComboFonts []  = {
  "Arial",
  "Bahnschrift",
  "Cambria",
  "Candara",
  "Comic Sans MS",
  "Consolas",
  "Constantia",
  "Corbel",
  "Courier New",
  "Ebrima",
  "Gabriola",
  "Gadugi",
  "Georgia",
  "Impact",
  "Lucida Console",
  "MS Gothic",
  "MV Boli",
  "Marlett",
  "Segoe Print",
  "Segoe Script",
  "Segoe UI",
  "SimSun",
  "Sylfaen",
  "Symbol",
  "Tahoma",
  "Times New Roman",
  "Trebuchet MS",
  "Verdana",
};

void page_combo(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_combo_sync);
	Model const *m = &g->model;
	g_page_header(g, parent, "ComboBox", "Use a ComboBox to let the user pick one item from a drop-down list.");
	{
		XentNodeId  ex              = g_example(g, parent, "A ComboBox with items defined inline.");
		GalleryDemo demo            = g_demo_card(g, ex);
		XentNodeId  row             = demo.items;
		XentNodeId  cb              = flux_combo_init(&(FluxComboBoxDesc) {
		  .ctx            = gallery_ctx(g),
		  .store          = gallery_store(g),
		  .parent         = row,
		  .window         = flux_app_window(g->flux),
		  .text           = flux_app_text(g->flux),
		  .theme          = flux_app_theme(g->flux),
		  .items          = kComboColors,
		  .item_count     = 4,
		  .selected_index = m->combo_sel,
		  .placeholder    = "Pick a color",
		  .on_select      = on_combo,
		  .userdata       = gallery_bind(g, MSG_COMBO),
		});
		g->live_control [MSG_COMBO] = cb;
		g->live_output [MSG_COMBO]  = g_output(
		  g, demo.row,
		  m->combo_sel >= 0 ? gallery_fmt(g, "Selected: %s", kComboColors [m->combo_sel]) : "Nothing selected yet"
		);
	}
	{
		XentNodeId  ex                   = g_example(g, parent, "A ComboBox with a long scrolling list.");
		GalleryDemo demo                 = g_demo_card(g, ex);
		XentNodeId  row                  = demo.items;
		int const   n                    = ( int ) (sizeof(kComboFonts) / sizeof(kComboFonts [0]));
		XentNodeId  cb                   = flux_combo_init(&(FluxComboBoxDesc) {
		  .ctx            = gallery_ctx(g),
		  .store          = gallery_store(g),
		  .parent         = row,
		  .window         = flux_app_window(g->flux),
		  .text           = flux_app_text(g->flux),
		  .theme          = flux_app_theme(g->flux),
		  .items          = kComboFonts,
		  .item_count     = n,
		  .selected_index = m->combo_font,
		  .placeholder    = "Pick a font",
		  .on_select      = on_combo,
		  .userdata       = gallery_bind(g, MSG_COMBO_FONT),
		});
		g->live_control [MSG_COMBO_FONT] = cb;
		g->live_output [MSG_COMBO_FONT]  = g_output(
		  g, demo.row,
		  m->combo_font >= 0 ? gallery_fmt(g, "Selected: %s", kComboFonts [m->combo_font]) : "Nothing selected yet"
		);
	}
}

void page_combo_sync(GalleryApp *g) {
	char const *color = g->model.combo_sel >= 0 ? gallery_fmt(g, "Selected: %s", kComboColors [g->model.combo_sel])
	                                            : "Nothing selected yet";
	xent_settext(gallery_ctx(g), g->live_output [MSG_COMBO], color);

	char const *font = g->model.combo_font >= 0 ? gallery_fmt(g, "Selected: %s", kComboFonts [g->model.combo_font])
	                                            : "Nothing selected yet";
	xent_settext(gallery_ctx(g), g->live_output [MSG_COMBO_FONT], font);
}
