#pragma once

#include <string>
#include <xent/view.hpp>


namespace fluxent::controls {

// Minimal ToggleSwitch control.
// State is stored in ViewData.text_content as "0" (off) / "1" (on) so it can be
// toggled by the fluxent InputHandler without requiring xent-core changes.
class ToggleSwitch : public xent::View {
public:
  ToggleSwitch() { data_->component_type = "ToggleSwitch"; }

  ToggleSwitch &is_on(bool on) {
    data_->is_checked = on;
    // Keep text_content for debugging or fallback if needed, but primary state
    // is is_checked
    data_->text_content = on ? "1" : "0";
    return *this;
  }

  bool is_on() const { return data_ && data_->is_checked; }

  // Fluent forwarding
  ToggleSwitch &width(float w) {
    xent::View::width(w);
    return *this;
  }
  ToggleSwitch &height(float h) {
    xent::View::height(h);
    return *this;
  }
  ToggleSwitch &margin(float m) {
    xent::View::margin(m);
    return *this;
  }
  ToggleSwitch &padding(float p) {
    xent::View::padding(p);
    return *this;
  }
  ToggleSwitch &padding(float v, float h) {
    xent::View::padding(v, h);
    return *this;
  }
  ToggleSwitch &padding(float t, float r, float b, float l) {
    xent::View::padding(t, r, b, l);
    return *this;
  }
  ToggleSwitch &flex_grow(float grow) {
    xent::View::flex_grow(grow);
    return *this;
  }

  template <typename T>
  ToggleSwitch &on_click(void (T::*method)(), T *instance) {
    xent::View::on_click(method, instance);
    return *this;
  }
};

} // namespace fluxent::controls
