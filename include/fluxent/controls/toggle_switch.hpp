#pragma once

#include <string>
#include <xent/view.hpp>

namespace fluxent::controls {

// Minimal ToggleSwitch control.
// State is stored in ViewData.text_content as "0" (off) / "1" (on) so it can be
// toggled by the fluxent InputHandler without requiring xent-core changes.
class ToggleSwitch : public xent::View {
public:
  ToggleSwitch() { data_->type = xent::ComponentType::ToggleSwitch; }

  ToggleSwitch &IsChecked(bool on) {
    data_->is_checked = on;
    return *this;
  }

  bool IsOn() const { return data_ && data_->is_checked; }

  // Fluent forwarding
  ToggleSwitch &Width(float w) {
    xent::View::Width(w);
    return *this;
  }
  ToggleSwitch &Height(float h) {
    xent::View::Height(h);
    return *this;
  }
  ToggleSwitch &Margin(float m) {
    xent::View::Margin(m);
    return *this;
  }
  ToggleSwitch &Padding(float p) {
    xent::View::Padding(p);
    return *this;
  }
  ToggleSwitch &Padding(float v, float h) {
    xent::View::Padding(v, h);
    return *this;
  }
  ToggleSwitch &Padding(float t, float r, float b, float l) {
    xent::View::Padding(t, r, b, l);
    return *this;
  }
  ToggleSwitch &FlexGrow(float grow) {
    xent::View::FlexGrow(grow);
    return *this;
  }

  template <typename T>
  ToggleSwitch &on_click(void (T::*method)(), T *instance) {
    xent::View::on_click(method, instance);
    return *this;
  }
};

} // namespace fluxent::controls
