#include "fluxent/input.hpp"
#include "fluxent/controls/layout.hpp"
#include "fluxent/config.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent
{

InputHandler::InputHandler() = default;
InputHandler::~InputHandler() = default;

static void CalculateScrollLayout(const xent::View *view, float &content_w, float &content_h,
                                  controls::ScrollBarLayout &h_bar,
                                  controls::ScrollBarLayout &v_bar)
{
  float max_child_right = 0.0f;
  float max_child_bottom = 0.0f;

  for (const auto &child : view->children)
  {
    float right = child->LayoutX() + child->LayoutWidth();
    float bottom = child->LayoutY() + child->LayoutHeight();
    if (right > max_child_right)
      max_child_right = right;
    if (bottom > max_child_bottom)
      max_child_bottom = bottom;
  }

  content_w = std::max(view->LayoutWidth(), max_child_right);
  content_h = std::max(view->LayoutHeight(), max_child_bottom);

  auto should_show = [](xent::ScrollBarVisibility vis, float content, float size)
  {
    if (vis == xent::ScrollBarVisibility::Hidden || vis == xent::ScrollBarVisibility::Disabled)
      return false;
    if (vis == xent::ScrollBarVisibility::Visible)
      return true;
    return content > size + fluxent::config::Input::ScrollVisibilityEpsilon;
  };

  float view_w = view->LayoutWidth();
  float view_h = view->LayoutHeight();

  bool show_h = should_show(view->horizontal_scrollbar_visibility, content_w, view_w);
  bool show_v = should_show(view->vertical_scrollbar_visibility, content_h, view_h);

  auto layout = controls::CalculateScrollViewLayout(view_w, view_h, content_w, content_h,
                                                    view->scroll_offset_x, view->scroll_offset_y,
                                                    show_h, show_v);

  h_bar = layout.h_bar;
  v_bar = layout.v_bar;
}
HitTestResult InputHandler::HitTest(const xent::View &root, const Point &position)
{
  HitTestResult result;
  result.view_data = nullptr;

  const xent::View *root_data = &root;
  if (root_data != nullptr)
  {
    HitTestRecursive(const_cast<xent::View *>(root_data), 0.0f, 0.0f, position, result);
  }

  return result;
}

bool InputHandler::HitTestRecursive(xent::View *view_data, float parent_x, float parent_y,
                                    const Point &position, HitTestResult &result)
{
  if (view_data == nullptr)
  {
    return false;
  }

  float x = view_data->LayoutX();
  float y = view_data->LayoutY();
  float w = view_data->LayoutWidth();
  float h = view_data->LayoutHeight();

  float abs_x = parent_x + x;
  float abs_y = parent_y + y;

  Rect bounds(abs_x, abs_y, w, h);

  if (!bounds.contains(position))
  {
    return false;
  }

  float child_parent_x = abs_x;
  float child_parent_y = abs_y;

  if (view_data->type == xent::ComponentType::ScrollView)
  {
    child_parent_x -= view_data->scroll_offset_x;
    child_parent_y -= view_data->scroll_offset_y;
  }

  if (view_data->type == xent::ComponentType::ScrollView)
  {
    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;

    CalculateScrollLayout(view_data, cw, ch, h_bar, v_bar);

    Point local_pos(position.x - abs_x, position.y - abs_y);

    bool hit_scrollbar = false;
    if (v_bar.visible && v_bar.bar_rect.contains(local_pos))
    {
      hit_scrollbar = true;
    }
    if (h_bar.visible && h_bar.bar_rect.contains(local_pos))
    {
      hit_scrollbar = true;
    }

    if (hit_scrollbar)
    {
      result.view_data = view_data;
      result.bounds = bounds;
      result.local_position = local_pos;
      return true;
    }
  }

  const auto &children = view_data->children;
  for (auto it = children.rbegin(); it != children.rend(); ++it)
  {
    if (HitTestRecursive(it->get(), child_parent_x, child_parent_y, position, result))
    {
      return true;
    }
  }

  result.view_data = view_data;
  result.bounds = bounds;
  result.local_position = Point(position.x - abs_x, position.y - abs_y);

  return true;
}

static bool UpdateSliderValue(xent::View *view, const Point &position, float abs_x, float abs_y)
{
  if (view->type != xent::ComponentType::Slider)
    return false;

  Rect bounds(abs_x, abs_y, view->LayoutWidth(), view->LayoutHeight());
  auto layout =
      controls::CalculateSliderLayout(bounds, view->min_value, view->max_value, view->value);

  if (layout.track_width <= 0)
    return false;

  float relative_x = position.x - layout.track_start_x;
  float pct = std::clamp(relative_x / layout.track_width, 0.0f, 1.0f);

  float range = view->max_value - view->min_value;
  float new_value = view->min_value + pct * range;

  if (view->step > 0)
  {
    float stepped =
        std::round((new_value - view->min_value) / view->step) * view->step + view->min_value;
    new_value = std::clamp(stepped, view->min_value, view->max_value);
  }

  if (view->value != new_value)
  {
    view->value = new_value;
    return true;
  }
  return false;
}

static Rect GetSliderThumbRect(const xent::View *view, float abs_x, float abs_y)
{
  Rect bounds(abs_x, abs_y, view->LayoutWidth(), view->LayoutHeight());
  auto layout =
      controls::CalculateSliderLayout(bounds, view->min_value, view->max_value, view->value);
  return layout.thumb_rect;
}

bool InputHandler::HandleVerticalScrollbarDrag(const MouseEvent &event)
{
  if (!is_dragging_v_scrollbar_ || !pressed_view_)
    return false;

  if (event.button == MouseButton::Left && !event.is_down)
  {
    is_dragging_v_scrollbar_ = false;
    pressed_view_ = nullptr;
    if (on_invalidate_)
      on_invalidate_();
    return true;
  }

  float dy = event.position.y - last_mouse_position_.y;
  last_mouse_position_ = event.position;

  float cw, ch;
  controls::ScrollBarLayout h_bar, v_bar;
  CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

  if (v_bar.visible && v_bar.track_rect.height > v_bar.thumb_rect.height)
  {
    float track_scrollable = v_bar.track_rect.height - v_bar.thumb_rect.height;
    float max_offset = ch - (pressed_view_->LayoutHeight() -
                             (h_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0));

    if (max_offset > 0 && track_scrollable > 0)
    {
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
  return true;
}

bool InputHandler::HandleHorizontalScrollbarDrag(const MouseEvent &event)
{
  if (!is_dragging_h_scrollbar_ || !pressed_view_)
    return false;

  if (event.button == MouseButton::Left && !event.is_down)
  {
    is_dragging_h_scrollbar_ = false;
    pressed_view_ = nullptr;
    if (on_invalidate_)
      on_invalidate_();
    return true;
  }

  float dx = event.position.x - last_mouse_position_.x;
  last_mouse_position_ = event.position;

  float cw, ch;
  controls::ScrollBarLayout h_bar, v_bar;
  CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

  if (h_bar.visible && h_bar.track_rect.width > h_bar.thumb_rect.width)
  {
    float track_scrollable = h_bar.track_rect.width - h_bar.thumb_rect.width;
    float max_offset = cw - (pressed_view_->LayoutWidth() -
                             (v_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0));

    if (max_offset > 0 && track_scrollable > 0)
    {
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
  return true;
}

bool InputHandler::HandleActiveTextBoxUpdate(const MouseEvent &event)
{
  if (!pressed_view_ || pressed_view_->type != xent::ComponentType::TextBox)
    return false;

  Point local = {event.position.x - pressed_view_bounds_.x,
                 event.position.y - pressed_view_bounds_.y};

  float px = local.x - pressed_view_->padding_left + pressed_view_->scroll_offset_x;
  float py = local.y - pressed_view_->padding_top;
  float content_width = fluxent::config::Text::HitTestExtent;

  auto hit_func = xent::GetTextHitTestFunc();
  if (hit_func)
  {
    int idx =
        hit_func(pressed_view_->text_content, pressed_view_->font_size, content_width, px, py);
    if (idx >= 0)
    {
      text_edit_.SetFocusedView(pressed_view_);
      text_edit_.HandleSelectionDrag(idx);
    }
  }

  if (!event.is_down && event.button == MouseButton::None)
    return true;

  return false;
}

bool InputHandler::HandlePressedViewPressLogic(const HitTestResult &hit_result,
                                               const MouseEvent &event)
{
  if (pressed_view_ && pressed_view_->type == xent::ComponentType::TextBox)
  {
    auto hit_func = xent::GetTextHitTestFunc();
    if (hit_func)
    {
      float px = hit_result.local_position.x - pressed_view_->padding_left +
                 pressed_view_->scroll_offset_x;
      float py = hit_result.local_position.y - pressed_view_->padding_top;
      float content_width = fluxent::config::Text::HitTestExtent;

      int idx =
          hit_func(pressed_view_->text_content, pressed_view_->font_size, content_width, px, py);
      if (idx >= 0)
      {
        text_edit_.SetFocusedView(pressed_view_);
        text_edit_.HandleSelectionAt(idx, event.click_count, event.shift);
      }
    }
  }

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::ScrollView)
  {
    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;
    CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

    Point local_pos = hit_result.local_position;

    if (v_bar.visible && v_bar.track_rect.contains(local_pos))
    {
      if (v_bar.thumb_rect.contains(local_pos))
      {
        is_dragging_v_scrollbar_ = true;
        drag_start_mouse_pos_ = event.position;
        return true;
      }
      else
      {
        float page_size = pressed_view_->LayoutHeight();
        float max_offset = ch - (pressed_view_->LayoutHeight() -
                                 (h_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0));

        if (local_pos.y < v_bar.thumb_rect.y)
        {
          pressed_view_->scroll_offset_y -= page_size;
        }
        else if (local_pos.y > v_bar.thumb_rect.bottom())
        {
          pressed_view_->scroll_offset_y += page_size;
        }

        if (pressed_view_->scroll_offset_y < 0)
          pressed_view_->scroll_offset_y = 0;
        if (pressed_view_->scroll_offset_y > max_offset)
          pressed_view_->scroll_offset_y = max_offset;

        if (on_invalidate_)
          on_invalidate_();
        return true;
      }
    }

    if (h_bar.visible && h_bar.track_rect.contains(local_pos))
    {
      if (h_bar.thumb_rect.contains(local_pos))
      {
        is_dragging_h_scrollbar_ = true;
        drag_start_mouse_pos_ = event.position;
        return true;
      }
      else
      {
        float page_size = pressed_view_->LayoutWidth();
        float max_offset = cw - (pressed_view_->LayoutWidth() -
                                 (v_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0));

        if (local_pos.x < h_bar.thumb_rect.x)
        {
          pressed_view_->scroll_offset_x -= page_size;
        }
        else if (local_pos.x > h_bar.thumb_rect.right())
        {
          pressed_view_->scroll_offset_x += page_size;
        }

        if (pressed_view_->scroll_offset_x < 0)
          pressed_view_->scroll_offset_x = 0;
        if (pressed_view_->scroll_offset_x > max_offset)
          pressed_view_->scroll_offset_x = max_offset;

        if (on_invalidate_)
          on_invalidate_();
        return true;
      }
    }
  }

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider)
  {
    float abs_x = hit_result.bounds.x;
    float abs_y = hit_result.bounds.y;

    Rect thumb_rect = GetSliderThumbRect(pressed_view_, abs_x, abs_y);
    const float inflate = fluxent::config::Input::SliderThumbHitInflate;
    thumb_rect.x -= inflate;
    thumb_rect.width += inflate * 2.0f;
    thumb_rect.y -= inflate;
    thumb_rect.height += inflate * 2.0f;

    if (thumb_rect.contains(event.position))
    {
      is_dragging_slider_thumb_ = true;
      float current_thumb_center_x =
          thumb_rect.x + inflate + (thumb_rect.width - inflate * 2.0f) / 2;
      slider_thumb_drag_offset_ = event.position.x - current_thumb_center_x;
    }
    else
    {
      is_dragging_slider_thumb_ = false;
      if (UpdateSliderValue(pressed_view_, event.position, abs_x, abs_y))
      {
        if (on_invalidate_)
          on_invalidate_();
      }
    }
    return true;
  }

  return false;
}

bool InputHandler::HandleWheelScroll(const xent::View &root, const HitTestResult &hit_result,
                                     const MouseEvent &event)
{
  if (std::abs(event.wheel_delta_y) > fluxent::config::Input::WheelEpsilon ||
      std::abs(event.wheel_delta_x) > fluxent::config::Input::WheelEpsilon)
  {
    xent::View *target = hit_result.view_data;
    if (!target && hovered_view_)
      target = hovered_view_;

    while (target)
    {
      if (target->type == xent::ComponentType::ScrollView)
      {
        break;
      }
      target = target->parent;
    }

    if (target)
    {
      float line_height = fluxent::config::Input::WheelScrollSpeed;
      bool changed = false;

      if (std::abs(event.wheel_delta_y) > fluxent::config::Input::WheelEpsilon)
      {
        target->scroll_offset_y -= line_height * event.wheel_delta_y;
        changed = true;
      }

      if (std::abs(event.wheel_delta_x) > fluxent::config::Input::WheelEpsilon)
      {
        target->scroll_offset_x += line_height * event.wheel_delta_x;
        changed = true;
      }

      if (changed)
      {
        float cw, ch;
        controls::ScrollBarLayout h_bar_l, v_bar_l;
        CalculateScrollLayout(target, cw, ch, h_bar_l, v_bar_l);

        float max_offset_y =
            (std::max)(0.0f, ch - (target->LayoutHeight() -
                                   (h_bar_l.visible ? fluxent::config::Layout::ScrollBarSize : 0)));
        float max_offset_x =
            (std::max)(0.0f, cw - (target->LayoutWidth() -
                                   (v_bar_l.visible ? fluxent::config::Layout::ScrollBarSize : 0)));

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
    return true;
  }
  return false;
}

void InputHandler::UpdateHoverIfChanged(const HitTestResult &hit_result)
{
  auto current_hovered = hovered_view_;
  if (hit_result.view_data != current_hovered)
  {
    auto old_hovered = current_hovered;
    hovered_view_ = hit_result.view_data;

    if (on_hover_changed_)
    {
      on_hover_changed_(old_hovered, hit_result.view_data);
    }

    if (on_invalidate_)
    {
      on_invalidate_();
    }
  }
}

void InputHandler::HandleMouseEvent(const xent::View &root, const MouseEvent &event)
{
  show_focus_visuals_ = false;

  if (event.is_down)
  {
    dm_active_ = false;
  }

  if (HandleVerticalScrollbarDrag(event))
    return;

  if (HandleHorizontalScrollbarDrag(event))
    return;

  if (HandleActiveTextBoxUpdate(event))
    return;

  HitTestResult hit_result = HitTest(root, event.position);

  if (HandleWheelScroll(root, hit_result, event))
  {
    return;
  }

  UpdateHoverIfChanged(hit_result);

  if (event.button != MouseButton::None)
  {
    if (event.is_down)
    {
      pressed_view_ = hit_result.view_data;
      pressed_view_bounds_ = hit_result.bounds;
      drag_start_mouse_pos_ = event.position;

      auto previous_focus = focused_view_;
      focused_view_ = hit_result.view_data;
      focused_view_bounds_ = hit_result.bounds;
      text_edit_.SetFocusedView(focused_view_);

      if (focused_view_ != previous_focus)
      {
        bool enable_ime = (focused_view_ && focused_view_->type == xent::ComponentType::TextBox);
        bool prev_ime = (previous_focus && previous_focus->type == xent::ComponentType::TextBox);

        if (enable_ime != prev_ime)
        {
          if (on_ime_state_change_)
          {
            on_ime_state_change_(enable_ime);
          }
        }

        if (enable_ime && event.source == InputSource::Touch)
        {
          if (on_keyboard_)
          {
            on_keyboard_(true);
          }
        }
        else if (!enable_ime && prev_ime)
        {
          if (on_keyboard_)
          {
            on_keyboard_(false);
          }
        }

      if (HandlePressedViewPressLogic(hit_result, event))
      {
        return;
      }

      if (on_invalidate_)
      {
        on_invalidate_();
      }
    }
    else
    {
      auto pressed = pressed_view_;
      if (pressed)
      {
        if (pressed->type != xent::ComponentType::Slider && pressed == hit_result.view_data)
        {
          if (!dm_active_)
          {
            DispatchToView(pressed, event);
          }
        }
      }

      pressed_view_ = nullptr;
      is_dragging_h_scrollbar_ = false;
      is_dragging_v_scrollbar_ = false;
      is_dragging_slider_thumb_ = false;

      if (on_invalidate_)
      {
        on_invalidate_();
      }
    }
  }
  else
  {
    if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider)
    {
      if (is_dragging_slider_thumb_)
      {
        Point effective_pos = event.position;
        effective_pos.x -= slider_thumb_drag_offset_;

        if (UpdateSliderValue(pressed_view_, effective_pos, pressed_view_bounds_.x,
                              pressed_view_bounds_.y))
        {
          if (on_invalidate_)
            on_invalidate_();
        }
      }
      else
      {
        if (UpdateSliderValue(pressed_view_, event.position, pressed_view_bounds_.x,
                              pressed_view_bounds_.y))
        {
          if (on_invalidate_)
            on_invalidate_();
        }
      }
    }
    else
    {
      if (pressed_view_ && event.source == InputSource::Touch &&
          pressed_view_->type != xent::ComponentType::ScrollView &&
          pressed_view_->type != xent::ComponentType::Slider)
      {

        float dx = event.position.x - drag_start_mouse_pos_.x;
        float dy = event.position.y - drag_start_mouse_pos_.y;
        float dist_sq = dx * dx + dy * dy;
        const float slop = fluxent::config::Input::TouchSlop;
        const float kTouchSlopSq = slop * slop;

        if (dist_sq > kTouchSlopSq)
        {
          xent::View *p = pressed_view_->parent;
          while (p)
          {
            if (p->type == xent::ComponentType::ScrollView)
            {
              pressed_view_ = p;
              break;
            }
            p = p->parent;
          }
        }
      }

      if (pressed_view_ && pressed_view_->type == xent::ComponentType::ScrollView &&
          !is_dragging_h_scrollbar_ && !is_dragging_v_scrollbar_ && !dm_active_)
      {
        float dx = event.position.x - last_mouse_position_.x;
        float dy = event.position.y - last_mouse_position_.y;

        pressed_view_->scroll_offset_x -= dx;
        pressed_view_->scroll_offset_y -= dy;

        float cw, ch;
        controls::ScrollBarLayout h_bar, v_bar;
        CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

        float max_offset_y =
            (std::max)(0.0f, ch - (pressed_view_->LayoutHeight() -
                                   (h_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0)));
        float max_offset_x =
            (std::max)(0.0f, cw - (pressed_view_->LayoutWidth() -
                                   (v_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0)));

        if (pressed_view_->scroll_offset_x < 0)
          pressed_view_->scroll_offset_x = 0;
        if (pressed_view_->scroll_offset_x > max_offset_x)
          pressed_view_->scroll_offset_x = max_offset_x;

        if (pressed_view_->scroll_offset_y < 0)
          pressed_view_->scroll_offset_y = 0;
        if (pressed_view_->scroll_offset_y > max_offset_y)
          pressed_view_->scroll_offset_y = max_offset_y;

        if (on_invalidate_)
        {
          on_invalidate_();
        }
      }
    }
  }
  last_mouse_position_ = event.position;
}

void InputHandler::HandleDirectManipulation(xent::View &root, float x, float y, float scale,
                                            bool centering)
{
  xent::View *target = nullptr;

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::ScrollView)
  {
    target = pressed_view_;
  }
  else if (hovered_view_)
  {
    xent::View *p = hovered_view_;
    while (p)
    {
      if (p->type == xent::ComponentType::ScrollView)
      {
        target = p;
        break;
      }
      p = p->parent;
    }
  }

  if (!target && root.type == xent::ComponentType::ScrollView)
  {
    target = &root;
  }

  if (!target)
    return;

  if (!dm_active_)
  {
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

  float max_offset_y =
      (std::max)(0.0f, ch - (target->LayoutHeight() -
                             (h_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0)));
  float max_offset_x =
      (std::max)(0.0f, cw - (target->LayoutWidth() -
                             (v_bar.visible ? fluxent::config::Layout::ScrollBarSize : 0)));

  if (target->scroll_offset_y < 0)
    target->scroll_offset_y = 0;
  if (target->scroll_offset_y > max_offset_y)
    target->scroll_offset_y = max_offset_y;

  if (target->scroll_offset_x < 0)
    target->scroll_offset_x = 0;
  if (target->scroll_offset_x > max_offset_x)
    target->scroll_offset_x = max_offset_x;

  if (on_invalidate_)
  {
    on_invalidate_();
  }
}

void InputHandler::HandleKeyEvent(const xent::View &root, const KeyEvent &event)
{
  (void)root;

  if (!event.is_down)
    return;

  switch (event.virtual_key)
  {
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

  text_edit_.SetFocusedView(focused_view_);
  text_edit_.HandleKey(event);
}

void InputHandler::HandleCharEvent(const xent::View &root, wchar_t ch)
{
  (void)root;
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.HandleChar(ch);
}

bool InputHandler::DispatchToView(xent::View *view_data, const MouseEvent &event)
{
  if (!view_data)
    return false;
  (void)event;

  if (view_data->type == xent::ComponentType::ToggleSwitch ||
      view_data->type == xent::ComponentType::ToggleButton ||
      view_data->type == xent::ComponentType::CheckBox)
  {
    view_data->is_checked = !view_data->is_checked;

    if (view_data->type == xent::ComponentType::ToggleSwitch)
    {
      view_data->text_content = view_data->is_checked ? "1" : "0";
    }
  }
  else if (view_data->type == xent::ComponentType::RadioButton)
  {
    if (!view_data->is_checked)
    {
      view_data->is_checked = true;

      if (!view_data->group_name.empty())
      {
        if (auto parent = view_data->parent)
        {
          for (auto &sibling : parent->children)
          {
            if (sibling.get() != view_data && sibling->type == xent::ComponentType::RadioButton &&
                sibling->group_name == view_data->group_name)
            {
              sibling->is_checked = false;
            }
          }
        }
      }
    }
  }

  if (view_data->on_click_handler)
  {
    view_data->on_click_handler();
  }

  if (event.button == MouseButton::Right && !event.is_down &&
      view_data->type == xent::ComponentType::TextBox)
  {
    POINT pt;
    GetCursorPos(&pt);

    text_edit_.ResetTypingSequence();
    ShowContextMenu(pt.x, pt.y);
  }

  if (on_invalidate_)
  {
    on_invalidate_();
  }

  return true;
}

void InputHandler::CancelInteraction()
{
  pressed_view_ = nullptr;
  hovered_view_ = nullptr;
  is_dragging_h_scrollbar_ = false;
  is_dragging_v_scrollbar_ = false;
  is_dragging_slider_thumb_ = false;
  dm_active_ = false;

  if (on_invalidate_)
  {
    on_invalidate_();
  }
}

void InputHandler::SetCompositionText(const std::wstring &text)
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.SetCompositionText(text);
}

void InputHandler::EnsureCursorVisible()
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.EnsureCursorVisible();
}

void InputHandler::PerformSelectAll()
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.PerformSelectAll();
}

void InputHandler::PerformCopy()
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.PerformCopy();
}

void InputHandler::PerformPaste()
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.PerformPaste();
}

void InputHandler::PerformCut()
{
  text_edit_.SetFocusedView(focused_view_);
  text_edit_.PerformCut();
}

void InputHandler::ShowContextMenu(int x, int y)
{
  enum class ContextMenuId : UINT
  {
    Cut = 1,
    Copy = 2,
    Paste = 3,
    SelectAll = 4
  };

  HMENU hMenu = CreatePopupMenu();
  AppendMenuW(hMenu, MF_STRING, static_cast<UINT>(ContextMenuId::Cut), L"Cut");
  AppendMenuW(hMenu, MF_STRING, static_cast<UINT>(ContextMenuId::Copy), L"Copy");
  AppendMenuW(hMenu, MF_STRING, static_cast<UINT>(ContextMenuId::Paste), L"Paste");
  AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hMenu, MF_STRING, static_cast<UINT>(ContextMenuId::SelectAll), L"Select All");

  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
  {
    EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::Cut), MF_GRAYED);
    EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::Copy), MF_GRAYED);
    EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::Paste), MF_GRAYED);
    EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::SelectAll), MF_GRAYED);
  }
  else
  {
    if (focused_view_->cursor_start == focused_view_->cursor_end)
    {
      EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::Cut), MF_GRAYED);
      EnableMenuItem(hMenu, static_cast<UINT>(ContextMenuId::Copy), MF_GRAYED);
    }
  }

  HWND hwnd = GetActiveWindow();
  int command = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
  DestroyMenu(hMenu);

  switch (static_cast<ContextMenuId>(command))
  {
  case ContextMenuId::Cut:
    PerformCut();
    break;
  case ContextMenuId::Copy:
    PerformCopy();
    break;
  case ContextMenuId::Paste:
    PerformPaste();
    break;
  case ContextMenuId::SelectAll:
    PerformSelectAll();
    break;
  }
}

}
