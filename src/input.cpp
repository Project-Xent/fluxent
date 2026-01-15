// FluXent Input Handler implementation
#include "fluxent/input.hpp"

namespace fluxent {

InputHandler::InputHandler() = default;
InputHandler::~InputHandler() = default;

// Hit test
HitTestResult InputHandler::HitTest(const xent::View &root,
                                    const Point &position) {
  HitTestResult result;
  result.view_data = nullptr;

  auto root_data = root.Data();
  if (root_data) {
    HitTestRecursive(root_data, 0.0f, 0.0f, position, result);
  }

  return result;
}

bool InputHandler::HitTestRecursive(
    const std::shared_ptr<xent::ViewData> &view_data, float parent_x,
    float parent_y, const Point &position, HitTestResult &result) {
  if (!view_data)
    return false;

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

  const auto &children = view_data->children;
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if (HitTestRecursive(*it, abs_x, abs_y, position, result)) {
      return true;
    }
  }

  result.view_data = view_data;
  result.bounds = bounds;
  result.local_position = Point(position.x - abs_x, position.y - abs_y);

  return true;
}

// Event handling

void InputHandler::HandleMouseEvent(const xent::View &root,
                                    const MouseEvent &event) {
  show_focus_visuals_ = false;
  last_mouse_position_ = event.position;

  HitTestResult hit_result = HitTest(root, event.position);

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
        DispatchToView(pressed, event);
      }

      pressed_view_.reset();

      if (on_invalidate_) {
        on_invalidate_();
      }
    }
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

bool InputHandler::DispatchToView(
    const std::shared_ptr<xent::ViewData> &view_data, const MouseEvent &event) {
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
        if (auto parent = view_data->parent.lock()) {
          for (auto &sibling : parent->children) {
            if (sibling.get() != view_data.get() &&
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

} // namespace fluxent
