// FluXent Input Handler implementation
#include "fluxent/input.hpp"
#include "fluxent/controls/layout.hpp"
#include "fluxent/config.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent {

InputHandler::InputHandler() = default;
InputHandler::~InputHandler() = default;

// Hit test

// Helper to get scrollbar layout details matches renderer
// fluxent::controls::ScrollBarLayout is used now

// Simplified copy of Content Size Calc from Renderer (Consolidated logic TODO)
static void CalculateScrollLayout(const xent::View *view, float &content_w,
                                  float &content_h, controls::ScrollBarLayout &h_bar,
                                  controls::ScrollBarLayout &v_bar) {
  // 1. Content Size
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

  // 2. Visibility Policy
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

  // 3. Shared Layout Calculation
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
    // Cast away const because we need non-const pointer for result
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

  // Apply Scroll Offsets for Children Hit Test
  float child_parent_x = abs_x;
  float child_parent_y = abs_y;

  if (view_data->type == xent::ComponentType::ScrollView) {
    child_parent_x -= view_data->scroll_offset_x;
    child_parent_y -= view_data->scroll_offset_y;
  }

  // Prioritize ScrollBars if this is a ScrollView
  if (view_data->type == xent::ComponentType::ScrollView) {
    // Calculate absolute scrollbar rects
    float cw, ch;
    controls::ScrollBarLayout h_bar, v_bar;

    // CalculateScrollLayout relies on child layout relative to this view.
    // But rects returned are relative to this view's top-left.
    // We need to shift them by abs_x, abs_y to compare with absolute position.
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

// Helper to update slider value based on position
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

  // Optional: Snap to step
  if (view->step > 0) {
    float stepped =
        std::round((new_value - view->min_value) / view->step) * view->step +
        view->min_value;
    new_value = std::clamp(stepped, view->min_value, view->max_value);
  }

  if (view->value != new_value) {
    view->value = new_value;
    return true; // value changed
  }
  return false;
}

// Helper to get slider thumb rect (matches renderer)
static Rect GetSliderThumbRect(const xent::View *view, float abs_x,
                               float abs_y) {
  Rect bounds(abs_x, abs_y, view->LayoutWidth(), view->LayoutHeight());
  auto layout = controls::CalculateSliderLayout(bounds, view->min_value,
                                                view->max_value, view->value);
  return layout.thumb_rect;
}

// Helper to get scrollbar layout details matches renderer

void InputHandler::HandleMouseEvent(const xent::View &root,
                                    const MouseEvent &event) {
  show_focus_visuals_ = false;

  // Reset DM state on new interaction if needed?
  // Or handle "End" of DM separately.
  // For now, if we get a MouseDown, assume DM finished or is superseded if it was mouse emulation.
  // But with DM enabled, WM_POINTERDOWN might trigger DM and we might not get MouseEvents if consumed?
  // Our Window::OnPointer returns 0 to consume Touch.
  // So HandleMouseEvent won't be called for Touch!
  // It IS called for real mouse.

  // If MouseEvent comes from Pen/Mouse, we might want to ensure DM isn't stuck.
  if (event.is_down) {
     dm_active_ = false; 
  }

  // Dragging Logic First
  if (is_dragging_v_scrollbar_ && pressed_view_) {
    if (event.button == MouseButton::Left && !event.is_down) {
      // Mouse Up - Stop Dragging
      is_dragging_v_scrollbar_ = false;
      pressed_view_ = nullptr;
      if (on_invalidate_)
        on_invalidate_();
      return;
    }

    // Drag Move
    float dy = event.position.y - last_mouse_position_.y; // Delta of mouse
    last_mouse_position_ = event.position;

    // Recalculate context to map pixel delta to offset delta
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

        // Clamp
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
    return;
  }

  // Normal Hit Testing
  HitTestResult hit_result = HitTest(root, event.position);

  // Wheel Scrolling
  if (std::abs(event.wheel_delta_y) > 0.001f ||
      std::abs(event.wheel_delta_x) > 0.001f) {
    // Find scrollable view (hovered or parent of hovered)
    xent::View *target = hit_result.view_data;
    // Start from hovered view if no direct target (which shouldn't really
    // happen with current logic, but safe fallback)
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

      // Vertical Scroll
      if (std::abs(event.wheel_delta_y) > 0.001f) {
        target->scroll_offset_y -= line_height * event.wheel_delta_y;
        changed = true;
      }

      // Horizontal Scroll
      if (std::abs(event.wheel_delta_x) > 0.001f) {
        // Normally wheel_delta_x positive means tilt right -> scroll right
        // (offset increases)
        target->scroll_offset_x += line_height * event.wheel_delta_x;
        changed = true;
      }

      if (changed) {
        // Clamp
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
      focused_view_ = hit_result.view_data;

      // Check for ScrollBar hit (Thumb dragging OR Track paging)
      if (pressed_view_ &&
          pressed_view_->type == xent::ComponentType::ScrollView) {
        float cw, ch;
        controls::ScrollBarLayout h_bar, v_bar;
        CalculateScrollLayout(pressed_view_, cw, ch, h_bar, v_bar);

        Point local_pos = hit_result.local_position;

        // Vertical ScrollBar Logic
        if (v_bar.visible && v_bar.track_rect.contains(local_pos)) {
          if (v_bar.thumb_rect.contains(local_pos)) {
            is_dragging_v_scrollbar_ = true;
            drag_start_mouse_pos_ = event.position;
            // Don't process slider logic or panning
            return;
          } else {
            // Track Click (Page Up/Down)
            float page_size = pressed_view_->LayoutHeight();
            float max_offset = ch - (pressed_view_->LayoutHeight() -
                                     (h_bar.visible ? 12.0f : 0));

            if (local_pos.y < v_bar.thumb_rect.y) {
              // Page Up
              pressed_view_->scroll_offset_y -= page_size;
            } else if (local_pos.y > v_bar.thumb_rect.bottom()) {
              // Page Down
              pressed_view_->scroll_offset_y += page_size;
            }

            // Clamp
            if (pressed_view_->scroll_offset_y < 0)
              pressed_view_->scroll_offset_y = 0;
            if (pressed_view_->scroll_offset_y > max_offset)
              pressed_view_->scroll_offset_y = max_offset;

            if (on_invalidate_)
              on_invalidate_();
            return;
          }
        }

        // Horizontal ScrollBar Logic
        if (h_bar.visible && h_bar.track_rect.contains(local_pos)) {
          if (h_bar.thumb_rect.contains(local_pos)) {
            is_dragging_h_scrollbar_ = true;
            drag_start_mouse_pos_ = event.position;
            return;
          } else {
            // Track Click (Page Left/Right)
            float page_size = pressed_view_->LayoutWidth();
            float max_offset = cw - (pressed_view_->LayoutWidth() -
                                     (v_bar.visible ? 12.0f : 0));

            if (local_pos.x < h_bar.thumb_rect.x) {
              // Page Left
              pressed_view_->scroll_offset_x -= page_size;
            } else if (local_pos.x > h_bar.thumb_rect.right()) {
              // Page Right
              pressed_view_->scroll_offset_x += page_size;
            }

            // Clamp
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

      // ... inside HandleMouseEvent ...

      // Initial click on slider
      if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider) {
        float abs_x = hit_result.bounds.x;
        float abs_y = hit_result.bounds.y;

        Rect thumb_rect = GetSliderThumbRect(pressed_view_, abs_x, abs_y);
        // Expand thumb hit target slightly for touch
        thumb_rect.x -= 10;
        thumb_rect.width += 20;
        thumb_rect.y -= 10;
        thumb_rect.height += 20;

        if (thumb_rect.contains(event.position)) {
          is_dragging_slider_thumb_ = true;
          // Center of thumb current pos
          float current_thumb_center_x =
              thumb_rect.x + 10 + (thumb_rect.width - 20) / 2;
          slider_thumb_drag_offset_ = event.position.x - current_thumb_center_x;
        } else {
          // Clicked on track -> Jump
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
      // Mouse Up
      auto pressed = pressed_view_;
      if (pressed) {
        if (pressed->type != xent::ComponentType::Slider &&
            pressed == hit_result.view_data) {
          DispatchToView(pressed, event);
        }
      }

      pressed_view_ = nullptr;
      is_dragging_h_scrollbar_ = false;
      is_dragging_v_scrollbar_ = false;
      is_dragging_slider_thumb_ = false; // Reset

      if (on_invalidate_) {
        on_invalidate_();
      }
    }
  } else {
    // Mouse Move
    if (pressed_view_ && pressed_view_->type == xent::ComponentType::Slider) {
      if (is_dragging_slider_thumb_) {
        // Apply offset
        Point effective_pos = event.position;
        effective_pos.x -= slider_thumb_drag_offset_;

        if (UpdateSliderValue(pressed_view_, effective_pos,
                              pressed_view_bounds_.x, pressed_view_bounds_.y)) {
          if (on_invalidate_)
            on_invalidate_();
        }
      } else {
        // Track drag (Jump to finger)
        if (UpdateSliderValue(pressed_view_, event.position,
                              pressed_view_bounds_.x, pressed_view_bounds_.y)) {
          if (on_invalidate_)
            on_invalidate_();
        }
      }
    } else {
      // Touch Panning Bubble-Up
      if (pressed_view_ && event.source == InputSource::Touch &&
          pressed_view_->type != xent::ComponentType::ScrollView &&
          pressed_view_->type != xent::ComponentType::Slider) {

        // Touch Slop Check
        float dx = event.position.x - drag_start_mouse_pos_.x;
        float dy = event.position.y - drag_start_mouse_pos_.y;
        float dist_sq = dx * dx + dy * dy;
        const float slop = fluxent::config::Input::TouchSlop; 
        const float kTouchSlopSq = slop * slop;

        if (dist_sq > kTouchSlopSq) {
          // If we are moving with Touch on a non-scrollview ...
          xent::View *p = pressed_view_->parent;
          while (p) {
            if (p->type == xent::ComponentType::ScrollView) {
              pressed_view_ = p;

              // Reset last position to avoid jump?
              // We want smooth pickup.
              // But we alreay moved 'dist'. If we reset last_mouse to
              // event.pos, we lose that delta. Ideally we apply that delta NOW.
              // But simplified: effectively we start panning from HERE.
              break;
            }
            p = p->parent;
          }
        }
      }
      // ... panning logic ...

      if (pressed_view_ &&
          pressed_view_->type == xent::ComponentType::ScrollView &&
          !is_dragging_h_scrollbar_ && !is_dragging_v_scrollbar_) {
        // Panning (only if not dragging scrollbar)
        float dx = event.position.x - last_mouse_position_.x;
        float dy = event.position.y - last_mouse_position_.y;

        // Fix for last_mouse_position_ being updated early?
        // Actually, if last_mouse_position_ was updated at line 337 (before
        // this block), dx/dy are 0. We need to use a delta that works. Since we
        // can't easily undo line 337 without changing flow, let's look at
        // logic. Line 337: last_mouse_position_ = event.position; This
        // definitely breaks this block if it runs after. FIX: relying on the
        // fact that we fixed line 337 in a previous step? No I didn't. We must
        // rely on `last_mouse_position_` holding PREVIOUS value. But line 337
        // *always* runs. Changing logic to use specific delta requires knowing
        // previous. CRITICAL FIX in this block won't work if line 337 is
        // already executed. I will fix line 337 in a separate edit or use a
        // member 'prev_frame_position_'? No, I should fix line 337.

        // Assuming I will fix line 337, here is the logic:
        pressed_view_->scroll_offset_x -= dx;
        pressed_view_->scroll_offset_y -= dy;

        // Clamp
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
  // 1. Determine Target
  // If we are already panning, stick to the pressed_view_ (if it's a ScrollView)
  xent::View *target = nullptr;

  if (pressed_view_ && pressed_view_->type == xent::ComponentType::ScrollView) {
    target = pressed_view_;
  } else if (hovered_view_) {
    // If we're not dragging, find the scrollable ancestor of the hovered view
    xent::View *p = hovered_view_;
    while (p) {
      if (p->type == xent::ComponentType::ScrollView) {
        target = p;
        break;
      }
      p = p->parent;
    }
  }

  // If no target (e.g. pan started on empty space), maybe try root?
  if (!target && root.type == xent::ComponentType::ScrollView) {
    target = &root;
  }

  if (!target)
    return;

  // 2. Calculate Delta
  // DirectManipulation gives cumulative transform for the viewport.
  // We need delta since last frame.
  // Note: DirectManipulation resets on new contact usually, but we need to track
  // relative changes for our scroll offset.

  // Logic: The viewport moves 'x' and 'y'. This corresponds to how much the
  // content should move visually.
  // Panning content UP means viewport moves DOWN?
  // DirectManipulation X/Y usually represents the translation of the content.
  // If x increases, content moves right.
  // ScrollOffset X increasing moves content LEFT (standard scroll).
  // So Delta Scroll Offset = - Delta DM Translation.

  if (!dm_active_) {
    // First frame of manipulation
    dm_last_x_ = x;
    dm_last_y_ = y;
    dm_active_ = true;
    pressed_view_ = target; // Lock target for duration?
                            // Actually, DM doesn't send "End" explicit easily via simple update callback...
                            // We need to know when it stops. StatusChanged deals with Inertia/Ready.
                            // But for simple update loop:
  }
  
  // Heuristic: if gap is too large, reset? No, relying on simple delta.

  float dx = x - dm_last_x_;
  float dy = y - dm_last_y_;

  // Update State
  dm_last_x_ = x;
  dm_last_y_ = y;

  // Apply to ScrollView
  target->scroll_offset_x -= dx;
  target->scroll_offset_y -= dy;

  // Clamp
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
  case VK_LEFT:
  case VK_RIGHT:
  case VK_UP:
  case VK_DOWN:
    show_focus_visuals_ = true;
    break;
  default:
    break;
  }
}

bool InputHandler::DispatchToView(xent::View *view_data,
                                  const MouseEvent &event) {
  if (!view_data)
    return false;
  (void)event;

  // Built-in Toggle behavior.
  if (view_data->type == xent::ComponentType::ToggleSwitch ||
      view_data->type == xent::ComponentType::ToggleButton ||
      view_data->type == xent::ComponentType::CheckBox) {
    view_data->is_checked = !view_data->is_checked;

    // Sync text_content for ToggleSwitch legacy/debug support
    if (view_data->type == xent::ComponentType::ToggleSwitch) {
      view_data->text_content = view_data->is_checked ? "1" : "0";
    }
  } else if (view_data->type == xent::ComponentType::RadioButton) {
    if (!view_data->is_checked) {
      view_data->is_checked = true;

      // Uncheck others in group
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

} // namespace fluxent
