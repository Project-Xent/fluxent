// FluXent Input Handler implementation
#include "fluxent/input.hpp"

namespace fluxent {

InputHandler::InputHandler() = default;
InputHandler::~InputHandler() = default;

// Hit test
HitTestResult InputHandler::hit_test(const xent::View &root,
                                     const Point &position) {
  HitTestResult result;
  result.view_data = nullptr;

  auto root_data = root.data();
  if (root_data) {
    hit_test_recursive(root_data, 0.0f, 0.0f, position, result);
  }

  return result;
}

bool InputHandler::hit_test_recursive(
    const std::shared_ptr<xent::ViewData> &view_data, float parent_x,
    float parent_y, const Point &position, HitTestResult &result) {
  if (!view_data)
    return false;

  float x = view_data->layout_x();
  float y = view_data->layout_y();
  float w = view_data->layout_width();
  float h = view_data->layout_height();

  float abs_x = parent_x + x;
  float abs_y = parent_y + y;

  Rect bounds(abs_x, abs_y, w, h);

  if (!bounds.contains(position)) {
    return false;
  }

  const auto &children = view_data->children;
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if (hit_test_recursive(*it, abs_x, abs_y, position, result)) {
      return true;
    }
  }

  result.view_data = view_data;
  result.bounds = bounds;
  result.local_position = Point(position.x - abs_x, position.y - abs_y);

  return true;
}

// Event handling

void InputHandler::handle_mouse_event(const xent::View &root,
                                      const MouseEvent &event) {
  show_focus_visuals_ = false;
  last_mouse_position_ = event.position;

  HitTestResult hit_result = hit_test(root, event.position);

  auto current_hovered = hovered_view_.lock();
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
      focused_view_ = hit_result.view_data;

      if (on_invalidate_) {
        on_invalidate_();
      }
    } else {
      auto pressed = pressed_view_.lock();

      if (pressed && pressed == hit_result.view_data) {
        dispatch_to_view(pressed, event);
      }

      pressed_view_.reset();

      if (on_invalidate_) {
        on_invalidate_();
      }
    }
  }
}

void InputHandler::handle_key_event(const xent::View &root,
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

bool InputHandler::dispatch_to_view(
    const std::shared_ptr<xent::ViewData> &view_data, const MouseEvent &event) {
  if (!view_data)
    return false;
  (void)event;

  // Built-in Toggle behavior.
  if (view_data->component_type == "ToggleSwitch" ||
      view_data->component_type == "ToggleButton") {
    view_data->is_checked = !view_data->is_checked;

    // Sync text_content for ToggleSwitch legacy/debug support
    if (view_data->component_type == "ToggleSwitch") {
      view_data->text_content = view_data->is_checked ? "1" : "0";
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

} // namespace fluxent
