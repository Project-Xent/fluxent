#include "fluxent/input.hpp"
#include "fluxent/controls/layout.hpp"
#include "fluxent/config.hpp"
#include "fluxent/utils.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent {

// Helper for word selection
static int FindWordStart(const std::wstring& text, int index) {
    if (text.empty() || index < 0) return 0;
    if (index > (int)text.size()) index = (int)text.size();
    if (index == 0) return 0;
    
    auto is_delimiter = [](wchar_t c) {
        return iswspace(c) || iswpunct(c);
    };

    bool start_type = is_delimiter(text[index - 1]);
    
    while (index > 0) {
        if (is_delimiter(text[index - 1]) != start_type) break;
        index--;
    }
    return index;
}

static int FindWordEnd(const std::wstring& text, int index) {
    if (text.empty() || index < 0) return 0;
     
    auto is_delimiter = [](wchar_t c) {
        return iswspace(c) || iswpunct(c);
    };
    
    if (index >= (int)text.size()) return (int)text.size();
    
    bool start_type = is_delimiter(text[index]);
    
    while (index < (int)text.size()) {
         if (is_delimiter(text[index]) != start_type) break;
         index++;
     }
     return index;
}

InputHandler::InputHandler() = default;
InputHandler::~InputHandler() = default;

static void CalculateScrollLayout(const xent::View *view, float &content_w,
                                  float &content_h, controls::ScrollBarLayout &h_bar,
                                  controls::ScrollBarLayout &v_bar) {
  float max_child_right = 0.0f;
  float max_child_bottom = 0.0f;

  for (const auto &child : view->children) {
    float right = child->LayoutX() + child->LayoutWidth();
    float bottom = child->LayoutY() + child->LayoutHeight();
    if (right > max_child_right)
      max_child_right = right;
    if (bottom > max_child_bottom)
      max_child_bottom = bottom;
  }

  content_w = std::max(view->LayoutWidth(), max_child_right);
  content_h = std::max(view->LayoutHeight(), max_child_bottom);

  auto should_show = [](xent::ScrollBarVisibility vis, float content,
                        float size) {
    if (vis == xent::ScrollBarVisibility::Hidden ||
        vis == xent::ScrollBarVisibility::Disabled)
      return false;
    if (vis == xent::ScrollBarVisibility::Visible)
      return true;
    return content > size + 0.5f;
  };

  float view_w = view->LayoutWidth();
  float view_h = view->LayoutHeight();

  bool show_h =
      should_show(view->horizontal_scrollbar_visibility, content_w, view_w);
  bool show_v =
      should_show(view->vertical_scrollbar_visibility, content_h, view_h);

  auto layout = controls::CalculateScrollViewLayout(
      view_w, view_h, content_w, content_h, view->scroll_offset_x,
      view->scroll_offset_y, show_h, show_v);

  h_bar = layout.h_bar;
  v_bar = layout.v_bar;
}
HitTestResult InputHandler::HitTest(const xent::View &root,
                                    const Point &position) {
  HitTestResult result;
  result.view_data = nullptr;

  const xent::View *root_data = &root;
  if (root_data != nullptr) {
    HitTestRecursive(const_cast<xent::View *>(root_data), 0.0f, 0.0f, position,
                     result);
  }

  return result;
}

bool InputHandler::HitTestRecursive(xent::View *view_data, float parent_x,
                                    float parent_y, const Point &position,
                                    HitTestResult &result) {
  if (view_data == nullptr) {
    return false;
  }

  float x = view_data->LayoutX();
  float y = view_data->LayoutY();
  float w = view_data->LayoutWidth();
  float h = view_data->LayoutHeight();

  float abs_x = parent_x + x;
  float abs_y = parent_y + y;

  Rect bounds(abs_x, abs_y, w, h);


  if (!bounds.contains(position)) {
    return false;
  }

  float child_parent_x = abs_x;
  float child_parent_y = abs_y;

  if (view_data->type == xent::ComponentType::ScrollView) {
    child_parent_x -= view_data->scroll_offset_x;
    child_parent_y -= view_data->scroll_offset_y;
  }

  if (view_data->type == xent::ComponentType::ScrollView) {
    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;

    CalculateScrollLayout(view_data, cw, ch, h_bar, v_bar);

    Point local_pos(position.x - abs_x, position.y - abs_y);

    bool hit_scrollbar = false;
    if (v_bar.visible && v_bar.bar_rect.contains(local_pos)) {
      hit_scrollbar = true;
    }
    if (h_bar.visible && h_bar.bar_rect.contains(local_pos)) {
      hit_scrollbar = true;
    }

    if (hit_scrollbar) {
      result.view_data = view_data;
      result.bounds = bounds;
      result.local_position = local_pos;
      return true;
    }
  }

  const auto &children = view_data->children;
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if (HitTestRecursive(it->get(), child_parent_x, child_parent_y, position,
                         result)) {
      return true;
    }
  }

  result.view_data = view_data;
  result.bounds = bounds;
  result.local_position = Point(position.x - abs_x, position.y - abs_y);

  return true;
}

static bool UpdateSliderValue(xent::View *view, const Point &position,
                              float abs_x, float abs_y) {
  if (view->type != xent::ComponentType::Slider)
    return false;

  Rect bounds(abs_x, abs_y, view->LayoutWidth(), view->LayoutHeight());
  auto layout = controls::CalculateSliderLayout(bounds, view->min_value,
                                                view->max_value, view->value);

  if (layout.track_width <= 0)
    return false;

  float relative_x = position.x - layout.track_start_x;
  float pct = std::clamp(relative_x / layout.track_width, 0.0f, 1.0f);

  float range = view->max_value - view->min_value;
  float new_value = view->min_value + pct * range;

  if (view->step > 0) {
    float stepped =
        std::round((new_value - view->min_value) / view->step) * view->step +
        view->min_value;
    new_value = std::clamp(stepped, view->min_value, view->max_value);
  }

  if (view->value != new_value) {
    view->value = new_value;
    return true;
  }
  return false;
}

static Rect GetSliderThumbRect(const xent::View *view, float abs_x,
                               float abs_y) {
  Rect bounds(abs_x, abs_y, view->LayoutWidth(), view->LayoutHeight());
  auto layout = controls::CalculateSliderLayout(bounds, view->min_value,
                                                view->max_value, view->value);
  return layout.thumb_rect;
}

void InputHandler::HandleMouseEvent(const xent::View &root,
                                    const MouseEvent &event) {
  show_focus_visuals_ = false;

  if (event.is_down) {
     dm_active_ = false; 
  }

  if (is_dragging_v_scrollbar_ && pressed_view_) {
    if (event.button == MouseButton::Left && !event.is_down) {
      is_dragging_v_scrollbar_ = false;
      pressed_view_ = nullptr;
      if (on_invalidate_)
        on_invalidate_();
      return;
    }

    float dy = event.position.y - last_mouse_position_.y;
    last_mouse_position_ = event.position;

    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;
    CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

    if (v_bar.visible && v_bar.track_rect.height > v_bar.thumb_rect.height) {
      float track_scrollable =
          v_bar.track_rect.height - v_bar.thumb_rect.height;
      float max_offset =
          ch - (pressed_view_->LayoutHeight() - (h_bar.visible ? 12.0f : 0));

      if (max_offset > 0 && track_scrollable > 0) {
        float ratio = max_offset / track_scrollable;
        pressed_view_->scroll_offset_y += dy * ratio;

        if (pressed_view_->scroll_offset_y < 0)
          pressed_view_->scroll_offset_y = 0;
        if (pressed_view_->scroll_offset_y > max_offset)
          pressed_view_->scroll_offset_y = max_offset;
      }
    }

    if (on_invalidate_)
      on_invalidate_();
    return;
  }

  if (is_dragging_h_scrollbar_ && pressed_view_) {
    if (event.button == MouseButton::Left && !event.is_down) {
      is_dragging_h_scrollbar_ = false;
      pressed_view_ = nullptr;
      if (on_invalidate_)
        on_invalidate_();
      return;
    }

    float dx = event.position.x - last_mouse_position_.x;
    last_mouse_position_ = event.position;

    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;
    CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

    if (h_bar.visible && h_bar.track_rect.width > h_bar.thumb_rect.width) {
      float track_scrollable = h_bar.track_rect.width - h_bar.thumb_rect.width;
      float max_offset =
          cw - (pressed_view_->LayoutWidth() - (v_bar.visible ? 12.0f : 0));

      if (max_offset > 0 && track_scrollable > 0) {
        float ratio = max_offset / track_scrollable;
        pressed_view_->scroll_offset_x += dx * ratio;

        if (pressed_view_->scroll_offset_x < 0)
          pressed_view_->scroll_offset_x = 0;
        if (pressed_view_->scroll_offset_x > max_offset)
          pressed_view_->scroll_offset_x = max_offset;
      }
    }

    if (on_invalidate_)
      on_invalidate_();
    if (on_invalidate_)
      on_invalidate_();
    return;
  }

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::TextBox) {
      Point local = { event.position.x - pressed_view_bounds_.x, 
                      event.position.y - pressed_view_bounds_.y };
                      
      float px = local.x - pressed_view_->padding_left + pressed_view_->scroll_offset_x;
      float py = local.y - pressed_view_->padding_top;
      float content_width = 10000.0f;
      
      auto hit_func = xent::GetTextHitTestFunc();
      if (hit_func) {
           int idx = hit_func(pressed_view_->text_content, pressed_view_->font_size, content_width, px, py);
           if (idx >= 0) {
                if (pressed_view_->cursor_end != idx) {
                    pressed_view_->cursor_end = idx;
                    EnsureCursorVisible();
                    if (on_invalidate_) on_invalidate_();
                }
           }
      }

      if (event.is_down) {
      } else if (event.button != MouseButton::None) {
      } else {
          return;
      }
  }

  HitTestResult hit_result = HitTest(root, event.position);

  if (std::abs(event.wheel_delta_y) > 0.001f ||
      std::abs(event.wheel_delta_x) > 0.001f) {
    xent::View *target = hit_result.view_data;
    if (!target && hovered_view_)
      target = hovered_view_;

    while (target) {
      if (target->type == xent::ComponentType::ScrollView) {
        break;
      }
      target = target->parent;
    }

    if (target) {
      float line_height = fluxent::config::Input::WheelScrollSpeed;
      bool changed = false;

      if (std::abs(event.wheel_delta_y) > 0.001f) {
        target->scroll_offset_y -= line_height * event.wheel_delta_y;
        changed = true;
      }

      if (std::abs(event.wheel_delta_x) > 0.001f) {
        target->scroll_offset_x += line_height * event.wheel_delta_x;
        changed = true;
      }

      if (changed) {
        float cw, ch;
        controls::ScrollBarLayout h_bar_l, v_bar_l;
        CalculateScrollLayout(target, cw, ch, h_bar_l, v_bar_l);

        float max_offset_y =
            (std::max)(0.0f, ch - (target->LayoutHeight() -
                                 (h_bar_l.visible ? 12.0f : 0)));
        float max_offset_x = (std::max)(
            0.0f, cw - (target->LayoutWidth() - (v_bar_l.visible ? 12.0f : 0)));

        if (target->scroll_offset_y < 0)
          target->scroll_offset_y = 0;
        if (target->scroll_offset_y > max_offset_y)
          target->scroll_offset_y = max_offset_y;

        if (target->scroll_offset_x < 0)
          target->scroll_offset_x = 0;
        if (target->scroll_offset_x > max_offset_x)
          target->scroll_offset_x = max_offset_x;

        if (on_invalidate_)
          on_invalidate_();
      }
    }
    return;
  }

  auto current_hovered = hovered_view_;
  if (hit_result.view_data != current_hovered) {
    auto old_hovered = current_hovered;
    hovered_view_ = hit_result.view_data;

    if (on_hover_changed_) {
      on_hover_changed_(old_hovered, hit_result.view_data);
    }

    if (on_invalidate_) {
      on_invalidate_();
    }
  }

  if (event.button != MouseButton::None) {
    if (event.is_down) {
      pressed_view_ = hit_result.view_data;
      pressed_view_bounds_ = hit_result.bounds;
      drag_start_mouse_pos_ = event.position;
      
      auto previous_focus = focused_view_;
      focused_view_ = hit_result.view_data;
      focused_view_bounds_ = hit_result.bounds;
      
      if (focused_view_ != previous_focus) {
          bool enable_ime = (focused_view_ && focused_view_->type == xent::ComponentType::TextBox);
          bool prev_ime = (previous_focus && previous_focus->type == xent::ComponentType::TextBox);
          
          if (enable_ime != prev_ime) {
              if (on_ime_state_change_) {
                  on_ime_state_change_(enable_ime);
              }
          }

          if (enable_ime && event.source == InputSource::Touch) {
              if (on_keyboard_) {
                  on_keyboard_(true);
              }
          } else if (!enable_ime && prev_ime) {
              if (on_keyboard_) {
                  on_keyboard_(false);
              }
          }
      }

      if (pressed_view_ && pressed_view_->type == xent::ComponentType::TextBox) {
        auto hit_func = xent::GetTextHitTestFunc();
        if (hit_func) {
          float px = hit_result.local_position.x - pressed_view_->padding_left + pressed_view_->scroll_offset_x;
          float py = hit_result.local_position.y - pressed_view_->padding_top;
          float content_width = 10000.0f;

          int idx = hit_func(pressed_view_->text_content, pressed_view_->font_size, content_width, px, py);
          if (idx >= 0) {
              if (event.click_count == 2) {
                  std::wstring wtext = ToWide(pressed_view_->text_content);
                  
                  pressed_view_->cursor_start = FindWordStart(wtext, idx);
                  pressed_view_->cursor_end = FindWordEnd(wtext, idx);
              } else if (event.click_count == 3) {
                  pressed_view_->cursor_start = 0;
                  pressed_view_->cursor_start = 0;
                  std::wstring wtext = ToWide(pressed_view_->text_content);
                  pressed_view_->cursor_end = static_cast<int>(wtext.size());
              } else {
                  pressed_view_->cursor_end = idx;
                  if (!event.shift) { 
                     pressed_view_->cursor_start = idx;
                  }
              }
              EnsureCursorVisible();
              if (on_invalidate_) on_invalidate_();
          }
        }
      }


      // Check for ScrollBar hit (Thumb dragging OR Track paging)
      if (pressed_view_ &&
          pressed_view_->type == xent::ComponentType::ScrollView) {
        float cw, ch;
        controls::ScrollBarLayout h_bar, v_bar;
        CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

        Point local_pos = hit_result.local_position;

        if (v_bar.visible && v_bar.track_rect.contains(local_pos)) {
          if (v_bar.thumb_rect.contains(local_pos)) {
            is_dragging_v_scrollbar_ = true;
            drag_start_mouse_pos_ = event.position;
            return;
          } else {
            float page_size = pressed_view_->LayoutHeight();
            float max_offset = ch - (pressed_view_->LayoutHeight() -
                                     (h_bar.visible ? 12.0f : 0));

            if (local_pos.y < v_bar.thumb_rect.y) {
              pressed_view_->scroll_offset_y -= page_size;
            } else if (local_pos.y > v_bar.thumb_rect.bottom()) {
              pressed_view_->scroll_offset_y += page_size;
            }

            if (pressed_view_->scroll_offset_y < 0)
              pressed_view_->scroll_offset_y = 0;
            if (pressed_view_->scroll_offset_y > max_offset)
              pressed_view_->scroll_offset_y = max_offset;

            if (on_invalidate_)
              on_invalidate_();
            return;
          }
        }

        if (h_bar.visible && h_bar.track_rect.contains(local_pos)) {
          if (h_bar.thumb_rect.contains(local_pos)) {
            is_dragging_h_scrollbar_ = true;
            drag_start_mouse_pos_ = event.position;
            return;
          } else {
            float page_size = pressed_view_->LayoutWidth();
            float max_offset = cw - (pressed_view_->LayoutWidth() -
                                     (v_bar.visible ? 12.0f : 0));

            if (local_pos.x < h_bar.thumb_rect.x) {
              pressed_view_->scroll_offset_x -= page_size;
            } else if (local_pos.x > h_bar.thumb_rect.right()) {
              pressed_view_->scroll_offset_x += page_size;
            }

            if (pressed_view_->scroll_offset_x < 0)
              pressed_view_->scroll_offset_x = 0;
            if (pressed_view_->scroll_offset_x > max_offset)
              pressed_view_->scroll_offset_x = max_offset;

            if (on_invalidate_)
              on_invalidate_();
            return;
          }
        }
      }

      if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider) {
        float abs_x = hit_result.bounds.x;
        float abs_y = hit_result.bounds.y;

        Rect thumb_rect = GetSliderThumbRect(pressed_view_, abs_x, abs_y);
        thumb_rect.x -= 10;
        thumb_rect.width += 20;
        thumb_rect.y -= 10;
        thumb_rect.height += 20;

        if (thumb_rect.contains(event.position)) {
          is_dragging_slider_thumb_ = true;
          float current_thumb_center_x =
              thumb_rect.x + 10 + (thumb_rect.width - 20) / 2;
          slider_thumb_drag_offset_ = event.position.x - current_thumb_center_x;
        } else {
          is_dragging_slider_thumb_ = false;
          if (UpdateSliderValue(pressed_view_, event.position, abs_x, abs_y)) {
            if (on_invalidate_)
              on_invalidate_();
          }
        }
      }

      if (on_invalidate_) {
        on_invalidate_();
      }
    } else {
      auto pressed = pressed_view_;
      if (pressed) {
        if (pressed->type != xent::ComponentType::Slider &&
            pressed == hit_result.view_data) {
          if (!dm_active_) {
            DispatchToView(pressed, event);
          }
        }
      }

      pressed_view_ = nullptr;
      is_dragging_h_scrollbar_ = false;
      is_dragging_v_scrollbar_ = false;
      is_dragging_slider_thumb_ = false;

      if (on_invalidate_) {
        on_invalidate_();
      }
    }
  } else {
    if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider) {
      if (is_dragging_slider_thumb_) {
        Point effective_pos = event.position;
        effective_pos.x -= slider_thumb_drag_offset_;

        if (UpdateSliderValue(pressed_view_, effective_pos,
                              pressed_view_bounds_.x, pressed_view_bounds_.y)) {
          if (on_invalidate_)
            on_invalidate_();
        }
      } else {
        if (UpdateSliderValue(pressed_view_, event.position,
                              pressed_view_bounds_.x, pressed_view_bounds_.y)) {
          if (on_invalidate_)
            on_invalidate_();
        }
      }
    } else {
      if (pressed_view_ && event.source == InputSource::Touch &&
          pressed_view_->type != xent::ComponentType::ScrollView &&
          pressed_view_->type != xent::ComponentType::Slider) {

        float dx = event.position.x - drag_start_mouse_pos_.x;
        float dy = event.position.y - drag_start_mouse_pos_.y;
        float dist_sq = dx * dx + dy * dy;
        const float slop = fluxent::config::Input::TouchSlop; 
        const float kTouchSlopSq = slop * slop;

        if (dist_sq > kTouchSlopSq) {
          xent::View *p = pressed_view_->parent;
          while (p) {
            if (p->type == xent::ComponentType::ScrollView) {
              pressed_view_ = p;
              break;
            }
            p = p->parent;
          }
        }
      }

      if (pressed_view_ &&
          pressed_view_->type == xent::ComponentType::ScrollView &&
          !is_dragging_h_scrollbar_ && !is_dragging_v_scrollbar_ &&
          !dm_active_) {
        float dx = event.position.x - last_mouse_position_.x;
        float dy = event.position.y - last_mouse_position_.y;

        pressed_view_->scroll_offset_x -= dx;
        pressed_view_->scroll_offset_y -= dy;

        float cw, ch;
        controls::ScrollBarLayout h_bar, v_bar;
        CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

        float max_offset_y =
            (std::max)(0.0f, ch - (pressed_view_->LayoutHeight() -
                                 (h_bar.visible ? 12.0f : 0)));
        float max_offset_x = (std::max)(0.0f, cw - (pressed_view_->LayoutWidth() -
                                                  (v_bar.visible ? 12.0f : 0)));

        if (pressed_view_->scroll_offset_x < 0)
          pressed_view_->scroll_offset_x = 0;
        if (pressed_view_->scroll_offset_x > max_offset_x)
          pressed_view_->scroll_offset_x = max_offset_x;

        if (pressed_view_->scroll_offset_y < 0)
          pressed_view_->scroll_offset_y = 0;
        if (pressed_view_->scroll_offset_y > max_offset_y)
          pressed_view_->scroll_offset_y = max_offset_y;

        if (on_invalidate_) {
          on_invalidate_();
        }
      }
    }
  }
  last_mouse_position_ = event.position;
}

void InputHandler::HandleDirectManipulation(xent::View &root, float x, float y,
                                            float scale, bool centering) {
  xent::View *target = nullptr;

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::ScrollView) {
    target = pressed_view_;
  } else if (hovered_view_) {
    xent::View *p = hovered_view_;
    while (p) {
      if (p->type == xent::ComponentType::ScrollView) {
        target = p;
        break;
      }
      p = p->parent;
    }
  }

  if (!target && root.type == xent::ComponentType::ScrollView) {
    target = &root;
  }

  if (!target)
    return;

  if (!dm_active_) {
    dm_last_x_ = x;
    dm_last_y_ = y;
    dm_active_ = true;
    pressed_view_ = target;
  }

  float dx = x - dm_last_x_;
  float dy = y - dm_last_y_;

  dm_last_x_ = x;
  dm_last_y_ = y;

  target->scroll_offset_x -= dx;
  target->scroll_offset_y -= dy;

  float cw, ch;
  controls::ScrollBarLayout h_bar, v_bar;
  CalculateScrollLayout(target, cw, ch, h_bar, v_bar);

  float max_offset_y = (std::max)(
      0.0f, ch - (target->LayoutHeight() - (h_bar.visible ? 12.0f : 0)));
  float max_offset_x = (std::max)(
      0.0f, cw - (target->LayoutWidth() - (v_bar.visible ? 12.0f : 0)));

  if (target->scroll_offset_y < 0)
    target->scroll_offset_y = 0;
  if (target->scroll_offset_y > max_offset_y)
    target->scroll_offset_y = max_offset_y;

  if (target->scroll_offset_x < 0)
    target->scroll_offset_x = 0;
  if (target->scroll_offset_x > max_offset_x)
    target->scroll_offset_x = max_offset_x;

  if (on_invalidate_) {
    on_invalidate_();
  }
}

void InputHandler::HandleKeyEvent(const xent::View &root,
                                  const KeyEvent &event) {
  (void)root;

  if (!event.is_down)
    return;

  switch (event.virtual_key) {
  case VK_TAB:
    show_focus_visuals_ = true;
    break;
  case VK_LEFT:
  case VK_RIGHT:
  case VK_UP:
  case VK_DOWN:
    show_focus_visuals_ = true;
    break;
  default:
    break;
  }

  if (focused_view_ && focused_view_->type == xent::ComponentType::TextBox) {
    if (focused_view_->text_content.empty()) {
      focused_view_->cursor_start = 0;
      focused_view_->cursor_end = 0;
    }

    std::wstring wtext = ToWide(focused_view_->text_content);

    int cursor = focused_view_->cursor_end;
    bool changed = false;

    switch (event.virtual_key) {
    case VK_LEFT:
      if (cursor > 0)
        cursor--;
      if (!event.shift)
        focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
      break;
    case VK_RIGHT:
      if (cursor < static_cast<int>(wtext.size()))
        cursor++;
      if (!event.shift)
        focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
      break;
    case VK_HOME:
      cursor = 0;
      if (!event.shift)
        focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
      break;
    case VK_END:
      cursor = static_cast<int>(wtext.size());
      if (!event.shift)
        focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
      break;
      break;
    case VK_BACK:
      last_op_was_typing_ = false;
      PushUndoState();
      if (focused_view_->cursor_start != focused_view_->cursor_end) {
        int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
        int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
        wtext.erase(start, end - start);
        cursor = start;
        focused_view_->cursor_start = cursor;
        focused_view_->cursor_end = cursor;
        changed = true;
      } else if (cursor > 0) {
        wtext.erase(cursor - 1, 1);
        cursor--;
        focused_view_->cursor_start = cursor;
        focused_view_->cursor_end = cursor;
        changed = true;
      }
      break;
    case VK_DELETE:
       PushUndoState();
       if (focused_view_->cursor_start != focused_view_->cursor_end) {
        int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
        int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
        wtext.erase(start, end - start);
        cursor = start;
        focused_view_->cursor_start = cursor;
        focused_view_->cursor_end = cursor;
        changed = true;
      } else if (cursor < static_cast<int>(wtext.size())) {
        wtext.erase(cursor, 1);
        changed = true;
      }
      break;
    case VK_RETURN:
      if (focused_view_->on_submit) {
        focused_view_->on_submit(focused_view_->text_content);
      }
      break;
    case 'A':
      if (event.ctrl) {
        PerformSelectAll();
        changed = true;
      }
      break;
    case 'C':
      if (event.ctrl) {
        PerformCopy();
      }
      break;
    case 'V':
      if (event.ctrl) {
        PerformPaste();
        changed = true;
      }
      break;
    case 'X':
      if (event.ctrl) {
        PerformCut();
        changed = true;
      }
      break;

    case 'Z':
      if (event.ctrl) {
        Undo();
      }
      break;
    case 'Y':
      if (event.ctrl) {
        Redo();
      }
      break;
    }

    if (changed) {
      focused_view_->text_content = ToUtf8(wtext);

      if (focused_view_->on_text_changed) {
        focused_view_->on_text_changed(focused_view_->text_content);
      }
      if (changed) {
        EnsureCursorVisible();
        if (on_invalidate_)
          on_invalidate_();
      }
    }
  }
}

void InputHandler::HandleCharEvent(const xent::View &root, wchar_t ch) {
  (void)root;
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  if (ch < 32 && ch != VK_TAB)
    return;

  std::wstring wtext = ToWide(focused_view_->text_content);

  ULONGLONG now = GetTickCount64();
  bool is_space = (ch == ' ' || ch == VK_RETURN || ch == VK_TAB);
  
  if (last_op_was_typing_ && !is_space && (now - last_typing_time_ < 1000)) {
     last_typing_time_ = now;
  } else {
     PushUndoState();
     last_typing_time_ = now;
     last_op_was_typing_ = true;
  }

  if (focused_view_->cursor_start != focused_view_->cursor_end) {
    int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
    int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
    wtext.erase(start, end - start);
    focused_view_->cursor_end = start;
    focused_view_->cursor_start = start;
  }

  int cursor = focused_view_->cursor_end;
  if (cursor > static_cast<int>(wtext.size())) cursor = static_cast<int>(wtext.size());

  wtext.insert(cursor, 1, ch);
  cursor++;
  focused_view_->cursor_start = cursor;
  focused_view_->cursor_end = cursor;

  focused_view_->text_content = ToUtf8(wtext);

  if (focused_view_->on_text_changed) {
    focused_view_->on_text_changed(focused_view_->text_content);
  }
  
  EnsureCursorVisible();
  
  if (on_invalidate_)
    on_invalidate_();
}


bool InputHandler::DispatchToView(xent::View *view_data,
                                  const MouseEvent &event) {
  if (!view_data)
    return false;
  (void)event;

  if (view_data->type == xent::ComponentType::ToggleSwitch ||
      view_data->type == xent::ComponentType::ToggleButton ||
      view_data->type == xent::ComponentType::CheckBox) {
    view_data->is_checked = !view_data->is_checked;

    if (view_data->type == xent::ComponentType::ToggleSwitch) {
      view_data->text_content = view_data->is_checked ? "1" : "0";
    }
  } else if (view_data->type == xent::ComponentType::RadioButton) {
    if (!view_data->is_checked) {
      view_data->is_checked = true;

      if (!view_data->group_name.empty()) {
        if (auto parent = view_data->parent) {
          for (auto &sibling : parent->children) {
            if (sibling.get() != view_data &&
                sibling->type == xent::ComponentType::RadioButton &&
                sibling->group_name == view_data->group_name) {
              sibling->is_checked = false;
            }
          }
        }
      }
    }
  }

  if (view_data->on_click_handler) {
    view_data->on_click_handler();
  }
  
  if (event.button == MouseButton::Right && !event.is_down && 
      view_data->type == xent::ComponentType::TextBox) {
      POINT pt;
      GetCursorPos(&pt);
      
      last_op_was_typing_ = false;
      ShowContextMenu(pt.x, pt.y);
  }

  if (on_invalidate_) {
    on_invalidate_();
  }

  return true;
}

void InputHandler::CancelInteraction() {
  pressed_view_ = nullptr;
  hovered_view_ = nullptr;
  is_dragging_h_scrollbar_ = false;
  is_dragging_v_scrollbar_ = false;
  is_dragging_slider_thumb_ = false;
  dm_active_ = false;

  if (on_invalidate_) {
    on_invalidate_();
  }
}

void InputHandler::SetCompositionText(const std::wstring &text) {
  if (focused_view_ && focused_view_->type == xent::ComponentType::TextBox) {
    if (text.empty()) {
      focused_view_->composition_text.clear();
      focused_view_->composition_cursor = 0;
      focused_view_->composition_length = 0;
    } else {
      if (focused_view_->cursor_start != focused_view_->cursor_end) {
         std::wstring wtext = ToWide(focused_view_->text_content);

         
         int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
         int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
         
         if (start < static_cast<int>(wtext.size())) {
             if (end > static_cast<int>(wtext.size())) end = static_cast<int>(wtext.size());
             wtext.erase(start, end - start);
             
             // Convert back to UTF-8
             focused_view_->text_content = ToUtf8(wtext);
             
             focused_view_->cursor_start = start;
             focused_view_->cursor_end = start;
         }
      }

      // Convert to UTF-8
      focused_view_->composition_text = ToUtf8(text);
      // Simplified cursor: end of composition
      // To support moving cursor within composition, we'd need ImmGetCompositionString(GCS_CURSORPOS)
      // For now, assume cursor is at end
      focused_view_->composition_cursor = static_cast<int>(text.length()); 
      focused_view_->composition_length = static_cast<int>(text.length());
    }
    if (on_invalidate_) on_invalidate_();
  }
}


void InputHandler::EnsureCursorVisible() {
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  auto func = xent::GetTextCaretRectFunc();
  if (!func)
    return;

  float content_width = focused_view_->LayoutWidth() -
                        focused_view_->padding_left -
                        focused_view_->padding_right;
  // Use a large max_width to measure logical position without wrapping constraints
  float measure_width = 10000.0f;

  auto [rx, ry, rw, rh] = func(focused_view_->text_content,
                               focused_view_->font_size, measure_width,
                               focused_view_->cursor_end);

  float caret_x = rx; // Relative to Text Origin
  float visual_x = caret_x - focused_view_->scroll_offset_x;

  // Thresholds
  if (visual_x < 0) {
    // Scroll Left to align caret at left edge
    focused_view_->scroll_offset_x = caret_x;
  } else if (visual_x > content_width) {
    // Scroll Right to align caret at right edge
    // Add small padding so caret isn't glued to edge
    focused_view_->scroll_offset_x = caret_x - content_width + 10.0f;
  }

  if (focused_view_->scroll_offset_x < 0)
    focused_view_->scroll_offset_x = 0;
}

void InputHandler::PushUndoState() {
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
  
  EditHistory state;
  state.text = focused_view_->text_content;
  state.start = focused_view_->cursor_start;
  state.end = focused_view_->cursor_end;
  
  undo_stack_.push_back(state);
  
  // Cap stack size
  if (undo_stack_.size() > 50) {
      undo_stack_.erase(undo_stack_.begin());
  }
  
  // Clear redo stack on new action
  redo_stack_.clear();
}

void InputHandler::Undo() {
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
  if (undo_stack_.empty()) return;
  
  // Save current state to redo
  EditHistory current;
  current.text = focused_view_->text_content;
  current.start = focused_view_->cursor_start;
  current.end = focused_view_->cursor_end;
  redo_stack_.push_back(current);
  
  // Restore
  EditHistory prev = undo_stack_.back();
  undo_stack_.pop_back();
  
  focused_view_->text_content = prev.text;
  focused_view_->cursor_start = prev.start;
  focused_view_->cursor_end = prev.end;
  
  if (focused_view_->on_text_changed)
    focused_view_->on_text_changed(focused_view_->text_content);
    
  EnsureCursorVisible();
  if (on_invalidate_) on_invalidate_();
}

void InputHandler::Redo() {
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
  if (redo_stack_.empty()) return;
  
  // Save current (which was the "undo" state) back to undo stack
  EditHistory current;
  current.text = focused_view_->text_content;
  current.start = focused_view_->cursor_start;
  current.end = focused_view_->cursor_end;
  undo_stack_.push_back(current);
  
  // Restore
  EditHistory next = redo_stack_.back();
  redo_stack_.pop_back();
  
  focused_view_->text_content = next.text;
  focused_view_->cursor_start = next.start;
  focused_view_->cursor_end = next.end;
  
  if (focused_view_->on_text_changed)
        focused_view_->on_text_changed(focused_view_->text_content);

  EnsureCursorVisible();
  if (on_invalidate_) on_invalidate_();
}





void InputHandler::PerformSelectAll() {
    if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
    
    std::wstring wtext; 
    if (!focused_view_->text_content.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, nullptr, 0);
        if (len > 0) {
             focused_view_->cursor_end = len - 1;
        }
    } else {
        focused_view_->cursor_end = 0;
    }
    focused_view_->cursor_start = 0;
    if (on_invalidate_) on_invalidate_();
}

void InputHandler::PerformCopy() {
    if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
    if (focused_view_->cursor_start == focused_view_->cursor_end) return;

    // Convert to wstring first
    std::wstring wtext;
    if (!focused_view_->text_content.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, nullptr, 0);
        if (len > 0) {
            wtext.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, &wtext[0], len);
            wtext.resize(len - 1);
        }
    }
    
    int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
    int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
    
    if (start >= (int)wtext.size()) return;
    if (end > (int)wtext.size()) end = (int)wtext.size();
    
    std::wstring selection = wtext.substr(start, end - start);
    
    if (OpenClipboard(nullptr)) {
      EmptyClipboard();
      HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (selection.size() + 1) * sizeof(wchar_t));
      if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
          wcscpy_s(pMem, selection.size() + 1, selection.c_str());
          GlobalUnlock(hMem);
          SetClipboardData(CF_UNICODETEXT, hMem);
        }
      }
      CloseClipboard();
    }
}

void InputHandler::PerformPaste() {
    if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
    
    if (OpenClipboard(nullptr)) {
      HANDLE hData = GetClipboardData(CF_UNICODETEXT);
      if (hData) {
        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hData));
        if (pData) {
          std::wstring paste_text = pData;
          GlobalUnlock(hData);
          
          PushUndoState(); // Save state before paste
          
          std::wstring wtext;
           if (!focused_view_->text_content.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, nullptr, 0);
            if (len > 0) {
                wtext.resize(len - 1);
                MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, &wtext[0], len);
            }
          }
          
          int cursor = focused_view_->cursor_end;
          
          // Delete selection if any
          if (focused_view_->cursor_start != focused_view_->cursor_end) {
            int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
            int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
            if (start < (int)wtext.size()) {
                if (end > (int)wtext.size()) end = (int)wtext.size();
                wtext.erase(start, end - start);
                cursor = start;
            }
          }
           
          // Insert paste text
          if (cursor > (int)wtext.size()) cursor = (int)wtext.size();
          wtext.insert(cursor, paste_text);
          cursor += static_cast<int>(paste_text.size());
          
          // Convert back
          if (wtext.empty()) {
              focused_view_->text_content.clear();
          } else {
             int len = WideCharToMultiByte(CP_UTF8, 0, wtext.c_str(), -1, nullptr, 0, nullptr, nullptr);
             if (len > 0) {
                 std::string utf8(len, 0);
                 WideCharToMultiByte(CP_UTF8, 0, wtext.c_str(), -1, &utf8[0], len, nullptr, nullptr);
                 utf8.resize(len - 1);
                 focused_view_->text_content = utf8;
             }
          }

          focused_view_->cursor_start = cursor;
          focused_view_->cursor_end = cursor;
          
          if (focused_view_->on_text_changed)
             focused_view_->on_text_changed(focused_view_->text_content);
          
          EnsureCursorVisible();
          if (on_invalidate_) on_invalidate_();
        }
      }
      CloseClipboard();
    }
}

void InputHandler::PerformCut() {
    if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) return;
    if (focused_view_->cursor_start == focused_view_->cursor_end) return;
    
    PushUndoState();
    
    PerformCopy();
    
    std::wstring wtext;
    if (!focused_view_->text_content.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, nullptr, 0);
        if (len > 0) {
            wtext.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, focused_view_->text_content.c_str(), -1, &wtext[0], len);
            wtext.resize(len - 1);
        }
    }
    
    int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
    int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
    
    if (start < (int)wtext.size()) {
        if (end > (int)wtext.size()) end = (int)wtext.size();
        wtext.erase(start, end - start);
    }
    
    // Convert back
      if (wtext.empty()) {
          focused_view_->text_content.clear();
      } else {
         int len = WideCharToMultiByte(CP_UTF8, 0, wtext.c_str(), -1, nullptr, 0, nullptr, nullptr);
         if (len > 0) {
             std::string utf8(len - 1, 0);
             WideCharToMultiByte(CP_UTF8, 0, wtext.c_str(), -1, &utf8[0], len, nullptr, nullptr);
             focused_view_->text_content = utf8;
         }
      }

    focused_view_->cursor_start = start;
    focused_view_->cursor_end = start;
    
    if (focused_view_->on_text_changed)
        focused_view_->on_text_changed(focused_view_->text_content);
        
    EnsureCursorVisible();
    if (on_invalidate_) on_invalidate_();
}

void InputHandler::ShowContextMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"Cut");
    AppendMenuW(hMenu, MF_STRING, 2, L"Copy");
    AppendMenuW(hMenu, MF_STRING, 3, L"Paste");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 4, L"Select All");
    
    // Disable if invalid
    if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox) {
        EnableMenuItem(hMenu, 1, MF_GRAYED);
        EnableMenuItem(hMenu, 2, MF_GRAYED);
        EnableMenuItem(hMenu, 3, MF_GRAYED);
        EnableMenuItem(hMenu, 4, MF_GRAYED);
    } else {
         if (focused_view_->cursor_start == focused_view_->cursor_end) {
             EnableMenuItem(hMenu, 1, MF_GRAYED); // Cut
             EnableMenuItem(hMenu, 2, MF_GRAYED); // Copy
         }
         // Paste check? (Requires opening clipboard, maybe skip for MVP)
    }
    
    HWND hwnd = GetActiveWindow();
    int command = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
    
    switch (command) {
        case 1: PerformCut(); break;
        case 2: PerformCopy(); break;
        case 3: PerformPaste(); break;
        case 4: PerformSelectAll(); break;
    }
}

} // namespace fluxent
