#ifndef FLUX_CONTROLS_H
#define FLUX_CONTROLS_H

#if defined(_MSC_VER) && defined(FLUX_IMPORT)
  #define FLUX_DATA __declspec(dllimport)
#else
  #define FLUX_DATA
#endif

typedef struct FluxControlModule         FluxControlModule;

void                                     flux_controls_use(FluxControlModule const *module);
void                                     flux_controls_all(void);

extern FLUX_DATA FluxControlModule const flux_ctrl_container;
extern FLUX_DATA FluxControlModule const flux_ctrl_text;
extern FLUX_DATA FluxControlModule const flux_ctrl_button;
extern FLUX_DATA FluxControlModule const flux_ctrl_toggle;
extern FLUX_DATA FluxControlModule const flux_ctrl_checkbox;
extern FLUX_DATA FluxControlModule const flux_ctrl_radio;
extern FLUX_DATA FluxControlModule const flux_ctrl_switch;
extern FLUX_DATA FluxControlModule const flux_ctrl_slider;
extern FLUX_DATA FluxControlModule const flux_ctrl_textbox;
extern FLUX_DATA FluxControlModule const flux_ctrl_scroll;
extern FLUX_DATA FluxControlModule const flux_ctrl_image;
extern FLUX_DATA FluxControlModule const flux_ctrl_progress;
extern FLUX_DATA FluxControlModule const flux_ctrl_list;
extern FLUX_DATA FluxControlModule const flux_ctrl_tab;
extern FLUX_DATA FluxControlModule const flux_ctrl_card;
extern FLUX_DATA FluxControlModule const flux_ctrl_divider;
extern FLUX_DATA FluxControlModule const flux_ctrl_canvas;
extern FLUX_DATA FluxControlModule const flux_ctrl_password;
extern FLUX_DATA FluxControlModule const flux_ctrl_number;
extern FLUX_DATA FluxControlModule const flux_ctrl_link;
extern FLUX_DATA FluxControlModule const flux_ctrl_repeat;
extern FLUX_DATA FluxControlModule const flux_ctrl_progressring;
extern FLUX_DATA FluxControlModule const flux_ctrl_badge;
extern FLUX_DATA FluxControlModule const flux_ctrl_tooltip;
extern FLUX_DATA FluxControlModule const flux_ctrl_flyout;
extern FLUX_DATA FluxControlModule const flux_ctrl_menufly;
extern FLUX_DATA FluxControlModule const flux_ctrl_dropdown;
extern FLUX_DATA FluxControlModule const flux_ctrl_split;
extern FLUX_DATA FluxControlModule const flux_ctrl_combo;
extern FLUX_DATA FluxControlModule const flux_ctrl_expander;
extern FLUX_DATA FluxControlModule const flux_ctrl_exphead;
extern FLUX_DATA FluxControlModule const flux_ctrl_expbody;
extern FLUX_DATA FluxControlModule const flux_ctrl_infobar;
extern FLUX_DATA FluxControlModule const flux_ctrl_dialog;
extern FLUX_DATA FluxControlModule const flux_ctrl_dialogbody;
extern FLUX_DATA FluxControlModule const flux_ctrl_menubar;
extern FLUX_DATA FluxControlModule const flux_ctrl_menuitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_nav;
extern FLUX_DATA FluxControlModule const flux_ctrl_navitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_tabs;
extern FLUX_DATA FluxControlModule const flux_ctrl_tabitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_listitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_listbox;
extern FLUX_DATA FluxControlModule const flux_ctrl_gridview;
extern FLUX_DATA FluxControlModule const flux_ctrl_repeater;
extern FLUX_DATA FluxControlModule const flux_ctrl_itemsview;
extern FLUX_DATA FluxControlModule const flux_ctrl_flip;
extern FLUX_DATA FluxControlModule const flux_ctrl_pips;
extern FLUX_DATA FluxControlModule const flux_ctrl_suggest;
extern FLUX_DATA FluxControlModule const flux_ctrl_togglesplit;
extern FLUX_DATA FluxControlModule const flux_ctrl_radiogroup;
extern FLUX_DATA FluxControlModule const flux_ctrl_rating;
extern FLUX_DATA FluxControlModule const flux_ctrl_selector;
extern FLUX_DATA FluxControlModule const flux_ctrl_selectoritem;
extern FLUX_DATA FluxControlModule const flux_ctrl_breadcrumb;
extern FLUX_DATA FluxControlModule const flux_ctrl_crumb;
extern FLUX_DATA FluxControlModule const flux_ctrl_teachtip;
extern FLUX_DATA FluxControlModule const flux_ctrl_tree;
extern FLUX_DATA FluxControlModule const flux_ctrl_treeitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_refresh;
extern FLUX_DATA FluxControlModule const flux_ctrl_avatar;
extern FLUX_DATA FluxControlModule const flux_ctrl_pager;
extern FLUX_DATA FluxControlModule const flux_ctrl_splitview;
extern FLUX_DATA FluxControlModule const flux_ctrl_splitpane;
extern FLUX_DATA FluxControlModule const flux_ctrl_splitbody;
extern FLUX_DATA FluxControlModule const flux_ctrl_titlebar;
extern FLUX_DATA FluxControlModule const flux_ctrl_scrollbar;
extern FLUX_DATA FluxControlModule const flux_ctrl_looping;
extern FLUX_DATA FluxControlModule const flux_ctrl_loopitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_datepicker;
extern FLUX_DATA FluxControlModule const flux_ctrl_timepicker;
extern FLUX_DATA FluxControlModule const flux_ctrl_pickerflyout;
extern FLUX_DATA FluxControlModule const flux_ctrl_calendar;
extern FLUX_DATA FluxControlModule const flux_ctrl_calday;
extern FLUX_DATA FluxControlModule const flux_ctrl_colorpicker;
extern FLUX_DATA FluxControlModule const flux_ctrl_colorspectrum;
extern FLUX_DATA FluxControlModule const flux_ctrl_colorslider;
extern FLUX_DATA FluxControlModule const flux_ctrl_colheader;
extern FLUX_DATA FluxControlModule const flux_ctrl_colitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_commandbar;
extern FLUX_DATA FluxControlModule const flux_ctrl_appbarbutton;
extern FLUX_DATA FluxControlModule const flux_ctrl_appbartoggle;
extern FLUX_DATA FluxControlModule const flux_ctrl_appbarsep;
extern FLUX_DATA FluxControlModule const flux_ctrl_cmdmore;
extern FLUX_DATA FluxControlModule const flux_ctrl_colbutton;
extern FLUX_DATA FluxControlModule const flux_ctrl_groupheader;
extern FLUX_DATA FluxControlModule const flux_ctrl_comboitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_suggestitem;
extern FLUX_DATA FluxControlModule const flux_ctrl_menupresenter;
extern FLUX_DATA FluxControlModule const flux_ctrl_menurow;
extern FLUX_DATA FluxControlModule const flux_ctrl_cmdovf;
extern FLUX_DATA FluxControlModule const flux_ctrl_cmdovfrow;
extern FLUX_DATA FluxControlModule const flux_ctrl_navslot;
extern FLUX_DATA FluxControlModule const flux_ctrl_frame;
extern FLUX_DATA FluxControlModule const flux_ctrl_fonticon;

#endif
