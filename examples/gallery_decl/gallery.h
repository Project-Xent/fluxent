#ifndef GALLERY_H
#define GALLERY_H

#include "gallery_model.h"

#include <fluxent/fluxent.h>
#include <fluxent/flux_app.h>
#include <math.h>

XtkEl *page_header(XtkUi *ui, char const *title, char const *blurb);
XtkEl *example(XtkUi *ui, char const *caption, XtkEl *content);
XtkEl *output_col(XtkUi *ui, char const *value);
XtkEl *demo_card(XtkUi *ui, XtkEl *control, char const *output, XtkEl *extra);
XtkEl *page_category(XtkUi *ui, char const *title, char const *blurb);

XtkEl *page_home(XtkUi *ui, Model const *m);
XtkEl *page_button(XtkUi *ui, Model const *m);
XtkEl *page_split(XtkUi *ui, Model const *m);
XtkEl *page_checkbox(XtkUi *ui, Model const *m);
XtkEl *page_radio(XtkUi *ui, Model const *m);
XtkEl *page_combo(XtkUi *ui, Model const *m);
XtkEl *page_slider(XtkUi *ui, Model const *m);
XtkEl *page_scroll_bar(XtkUi *ui, Model const *m);
XtkEl *page_looping(XtkUi *ui, Model const *m);
XtkEl *page_date_picker(XtkUi *ui, Model const *m);
XtkEl *page_time_picker(XtkUi *ui, Model const *m);
XtkEl *page_calendar(XtkUi *ui, Model const *m);
XtkEl *page_color_picker(XtkUi *ui, Model const *m);
XtkEl *page_toggle(XtkUi *ui, Model const *m);
XtkEl *page_radio_buttons(XtkUi *ui, Model const *m);
XtkEl *page_rating(XtkUi *ui, Model const *m);
XtkEl *page_toggle_split(XtkUi *ui, Model const *m);
XtkEl *page_breadcrumb(XtkUi *ui, Model const *m);
XtkEl *page_selector_bar(XtkUi *ui, Model const *m);
XtkEl *page_teaching_tip(XtkUi *ui, Model const *m);
XtkEl *page_textbox(XtkUi *ui, Model const *m);
XtkEl *page_numberbox(XtkUi *ui, Model const *m);
XtkEl *page_typography(XtkUi *ui, Model const *m);
XtkEl *page_infobar(XtkUi *ui, Model const *m);
XtkEl *page_badge(XtkUi *ui, Model const *m);
XtkEl *page_progress(XtkUi *ui, Model const *m);
XtkEl *page_tooltip(XtkUi *ui, Model const *m);
XtkEl *page_image(XtkUi *ui, Model const *m);
XtkEl *page_person_picture(XtkUi *ui, Model const *m);
XtkEl *page_pager(XtkUi *ui, Model const *m);
XtkEl *page_split_view(XtkUi *ui, Model const *m);
XtkEl *page_title_bar(XtkUi *ui, Model const *m);
XtkEl *page_menus(XtkUi *ui, Model const *m);
XtkEl *page_tabview(XtkUi *ui, Model const *m);
XtkEl *page_expander(XtkUi *ui, Model const *m);
XtkEl *page_listview(XtkUi *ui, Model const *m);
XtkEl *page_gridview(XtkUi *ui, Model const *m);
XtkEl *page_listbox(XtkUi *ui, Model const *m);
XtkEl *page_tree_view(XtkUi *ui, Model const *m);
XtkEl *page_items_view(XtkUi *ui, Model const *m);
XtkEl *page_pull_refresh(XtkUi *ui, Model const *m);
XtkEl *page_flipview(XtkUi *ui, Model const *m);
XtkEl *page_autosuggest(XtkUi *ui, Model const *m);
XtkEl *page_dialog(XtkUi *ui, Model const *m);
XtkEl *page_settings(XtkUi *ui, Model const *m);

void   gallery_decl_update(void *model, XtkMsg msg);
XtkEl *gallery_decl_view(XtkUi *ui, void *model);
XtkEl *gallery_decl_page_view(XtkUi *ui, void *model);

#endif
