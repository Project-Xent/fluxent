# Changelog

Entries start at 0.3.1; earlier releases are described by their tags.

## 0.3.1

- applied xent-core style runs to DirectWrite layouts by range, covering family,
  size, weight, stretch and slant;
- applied trimming policies through `DWRITE_TRIMMING`, with an ellipsis sign
  that inherits the face and size of the text it truncates;
- applied line limits by resolving them from line metrics, so a limited box is
  both clipped when drawn and sized to the lines it will actually show;
- implemented the `shape` backend slot: an `IDWriteTextRenderer` captures
  `DWRITE_GLYPH_RUN` callbacks into positioned glyph runs, with clusters
  reported as UTF-8 byte offsets and faces exposed as opaque font cookies;
- reported the first line's baseline to layout, replacing the assumption that a
  text node's baseline sits at 80% of its height;
- **Fixed:** measurement ignored the requested font family and always used the
  renderer default, so text in any other family was measured against the wrong
  font and drawn wider than its box;
- extended the layout cache key with the run array, trimming policy and line
  limit;
- consolidated UTF-8/UTF-16 conversion into `text/flux_utf.h`. The textbox and
  the text layer had grown separate implementations, and only one of them
  validated continuation bytes and rejected overlong or surrogate encodings —
  the two could disagree about where a character starts;
- split the text layer along its seams: encoding, style-to-DirectWrite mapping
  and layout configuration, the format/layout caches, the xent backend, and the
  public drawing and hit-test API;
- made `multiline` a real mode rather than a stored flag: the field now wraps,
  hangs from the top, scrolls vertically, hit-tests in two dimensions, breaks
  the line on Enter, and navigates by visual line — Up/Down keep the column they
  set out from, Home/End address the line, and Ctrl+Home/End the buffer;
- kept Up/Down as value steps on NumberBox, which is never multiline;
- **Fixed:** a read-only field drew no selection at all, so its text could be
  selected but never seen to be. Selection now renders; only the caret is
  suppressed;
- carried the line cap and trimming policy through the display list, so a static
  label's `max_lines` finally reaches the renderer that can honour it;
- implemented `selectable` on the static label: press, drag, double-click,
  Ctrl+A, Ctrl+C, and a highlight that clears when focus leaves;
- moved putting plain text on the clipboard out of the textbox and into
  `text/flux_clipboard.h`, where a label can reach it too;
- added the standalone ScrollBar, ported from WinUI's `ScrollBar_Partial.cpp`
  and `DefaultScrollBarStyle`: line buttons, page regions, thumb drag, the
  three indicator modes, and the range coercion RangeBase performs — including
  the un-clamped value it remembers, so widening a range restores what a
  narrower one had cut off;
- turned scroll bar geometry into a value model shared with the ScrollViewer
  overlay. Both now resolve the same range (minimum, maximum, value, viewport)
  into the same parts; what they keep apart is what drives the expansion and
  how wide the bar reads across its axis;
- extracted hold-to-repeat timing into `behavior/flux_repeat_timer.h`. The
  RepeatButton had the only implementation, and the scroll bar's line and page
  buttons need the same Delay/Interval behaviour;
- linked the `Windows.Globalization` bindings, so date and calendar arithmetic
  can come from the same `wg::Calendar` WinUI itself uses;
- added the LoopingSelector, ported from WinUI's `LoopingSelector_Partial.cpp`
  and the templates in `DateTimePickerFlyout_themeresources.xaml`: a wheel that
  snaps to whole items so the centred one is the selection, endless looping over
  a panel it re-centres as it drifts, navigation buttons that appear on hover
  and repeat while held, tap-to-centre, and the arrow, page and Home/End keys.
  Selection commits only once the view settles, so a wheel mid-flick highlights
  nothing, as WinUI's does;
- gave the scroll viewport an opt-in vertical snap interval, enforced by
  DirectManipulation itself, so a flick lands on a boundary instead of settling
  and then sliding to one;
- generalized overlay chrome: a control that paints over its own children (the
  `record_overlay` pass) can now declare `chrome_hit`, and the input layer walks
  up from the deepest hit to give that ancestor the hover and the press. Without
  it an overlay is visible but unreachable, which is why the ScrollViewer had to
  carry its own version of both halves;
- gave both galleries a ScrollBar page and a LoopingSelector page;
- rebuilt the ColorPicker from the WinUI template and sources: a 256-pixel
  spectrum whose axes and third-dimension blending match WinUI's bitmaps, the
  44-pixel preview, 12-pixel slider tracks with an 18-pixel thumb, RGB/HSV,
  opacity and hex entries that apply while typing and roll back on blur, and
  the More button, hidden by default as `IsMoreButtonVisible` is;
- let a precision touchpad pan the LoopingSelector and the CalendarView by its
  own deltas, settling on an item or a month once input stops, while wheel
  notches still step;
- **Fixed:** hover coordinates ignored scroll offsets, so a control that tests
  sub-regions against them (calendar, flip view, pager, rating, title bar, the
  textbox and combo buttons) rejected or misplaced clicks once its page was
  scrolled;
- **Fixed:** the CalendarView arrows paged from the first visible cell, which
  usually belongs to the previous month, so the next arrow often stayed put;
- **Fixed:** showing a TeachingTip while it was still closing reopened it after
  the close event had already been delivered, leaving the tip and its owner
  disagreeing; requests during the close animation are now ignored, as in
  WinUI;
- removed the DatePicker and TimePicker placeholder: with no value each column
  shows its own name, as WinUI does;
- applied the TextBox `multiline` and `read-only` properties and the static
  text line cap, trimming, wrapping and selection properties from the host;
  a selectable label takes focus so Ctrl+C reaches it;
- built a TreeView's checks from its model, deriving each parent from its
  children, instead of clearing them whenever the items were replaced;
- added `FluxAppCfg.on_stop`, called after the message loop ends and before the
  app is torn down, so a worker thread that posts with `flux_app_post` can be
  stopped while the app still exists;
- **Fixed:** Ctrl+A in a Multiple or Extended list compared the key with the
  character `'A'` instead of `FLUX_KEY_A`, so it never matched;
- **Fixed:** a single-selection list only reported a click and waited for the
  app to push the index back, so a list whose app did not (the imperative
  gallery) could never be selected. It now selects as WinUI's Selector does and
  then raises the event;
- **Fixed:** every static label took pointer input for text selection, so a click
  on a list or tree row's text never reached the row. Labels take input only
  when `selectable` is on, as a TextBlock does with `IsTextSelectionEnabled`;
- scrolled a multiline TextBox with the wheel and touchpad, chaining what it
  cannot use to the scroller around it; it used to move only with the caret, so
  a read-only one could not scroll at all;
- aligned the TreeView with WinUI's template (`docs/audit/audit-treeview.md`):
  rows are 32 pixels with the 3/5 content padding, the multi-select box is the
  CheckBox renderer itself with its own hover and press, and a checked row
  layers the selected fill over the hover fill;
- **Fixed:** a ScrollViewer's content was laid out like any flex child and,
  shrinking by default, was compressed to the viewport, so once it outgrew the
  viewport there was nothing to scroll. Scroll nodes are now scrollers, which
  is what WinUI's measure-with-infinity on the scrolling axis amounts to;
- added `examples/installer_wizard`, a six-page declarative setup wizard: a
  read-only multiline license, a destination folder with a stand-in folder
  dialog, a three-state component tree, an editable ComboBox, a DatePicker and
  a vertical Slider, a worker thread reporting progress into a scrolling log
  with a trimmed current-file line, and a selectable summary with a hyperlink.

### Known limitations

Deferred to 0.3.2:

- a declarative ScrollViewer's offset cannot be driven from the model, so a log
  that grows does not follow its end;
- trimming ends a line with the ellipsis; DirectWrite's path-delimiter trimming,
  which elides the middle of a path, is not exposed by `XentTextTrim`.

### Renamed

Public names now follow `<pkg>_<module>_<verbobject>` with `comp`/`ctrl` abbreviations.

| Old | New |
|---|---|
| `flux_host_bind_app` | `flux_host_bindapp` |
| `flux_host_register_component` | `flux_host_registercomp` |
| `flux_host_set_focus` | `flux_host_setfocus` |
| `flux_host_set_providers` | `flux_host_setproviders` |
| `flux_host_take_invalidations` | `flux_host_takeinvalid` |
| `flux_image_resource_path` | `flux_image_respath` |
| `flux_input_pointer_inside` | `flux_input_pointerinside` |
| `flux_popup_acrylic_active` | `flux_popup_acrylicactive` |
| `flux_popup_acrylic_tint` | `flux_popup_acrylictint` |
| `flux_popup_lightdismiss_visible` | `flux_popup_lightdismissvisible` |
| `flux_popup_set_content` | `flux_popup_setcontent` |
| `flux_popup_set_size` | `flux_popup_setsize` |
| `flux_popup_update_position` | `flux_popup_reposition` |
| `flux_tooltip_on_hover` | `flux_tooltip_hover` |
| `flux_tooltip_set_theme` | `flux_tooltip_settheme` |
| `flux_theme_get_colors` | `flux_theme_colors` |
| `flux_font_weight_numeric` | `flux_font_weightnum` |
| `flux_to_d2d_color` | `flux_color_tod2d` |
| `flux_input_setfocus_programmatic` | flux_input_setfocus(.., true)  (merged; `flux_input_setfocus` gained a `bool programmatic` parameter) |
| `include/fluxent/flux_graphics.h` | `include/fluxent/flux_gfx.h` |

### Removed

- removed the navigation trace (`flux_nav_trace_*`, `include/fluxent/flux_nav_trace.h`):
  a debugging aid that wrote a `*.nav_trace.*.log` file next to every
  executable that used fluxent.
