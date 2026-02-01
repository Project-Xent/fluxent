#pragma once

#include <string>
#include <xent/view.hpp>

namespace fluxent::controls
{

class ToggleSwitch : public xent::View
{
public:
  ToggleSwitch() { type = xent::ComponentType::ToggleSwitch; }

  ToggleSwitch &IsChecked(bool on)
  {
    is_checked = on;
    return *this;
  }

  bool IsOn() const { return is_checked; }

  ToggleSwitch &Width(float w)
  {
    xent::View::Width(w);
    return *this;
  }
  ToggleSwitch &Height(float h)
  {
    xent::View::Height(h);
    return *this;
  }
  ToggleSwitch &Margin(float m)
  {
    xent::View::Margin(m);
    return *this;
  }
  ToggleSwitch &Padding(float p)
  {
    xent::View::Padding(p);
    return *this;
  }
  ToggleSwitch &Padding(float v, float h)
  {
    xent::View::Padding(v, h);
    return *this;
  }
  ToggleSwitch &Padding(float t, float r, float b, float l)
  {
    xent::View::Padding(t, r, b, l);
    return *this;
  }
  ToggleSwitch &FlexGrow(float grow)
  {
    xent::View::FlexGrow(grow);
    return *this;
  }

  template <typename T> ToggleSwitch &OnClick(void (T::*method)(), T *instance)
  {
    xent::View::OnClick(method, instance);
    return *this;
  }
};

} // namespace fluxent::controls
