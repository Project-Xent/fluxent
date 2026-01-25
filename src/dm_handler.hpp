#pragma once

#include <directmanipulation.h>
#include <fluxent/window.hpp>
#include <wrl.h>

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

namespace fluxent {

class DirectManipulationEventHandler
    : public RuntimeClass<
          RuntimeClassFlags<ClassicCom>,
          IDirectManipulationViewportEventHandler> {
public:
  DirectManipulationEventHandler(Window *window) : window_(window) {}

  IFACEMETHODIMP OnViewportStatusChanged(
      IDirectManipulationViewport *viewport,
      DIRECTMANIPULATION_STATUS current,
      DIRECTMANIPULATION_STATUS previous) override {
    
    // Dispatch Status Change
    window_->DispatchDirectManipulationStatusChanged(current);
    return S_OK;
  }

  IFACEMETHODIMP OnViewportUpdated(
      IDirectManipulationViewport *viewport) override {
    return S_OK;
  }

  IFACEMETHODIMP OnContentUpdated(
      IDirectManipulationViewport *viewport,
      IDirectManipulationContent *content) override {
    if (!window_)
      return S_OK;

    float xform[6];
    if (SUCCEEDED(content->GetContentTransform(xform, ARRAYSIZE(xform)))) {
      // 0: M11 (ScaleX), 3: M22 (ScaleY), 4: M31 (TransX), 5: M32 (TransY)
      float scale = xform[0];
      float x = xform[4];
      float y = xform[5];

      window_->DispatchDirectManipulationUpdate(x, y, scale, false);
    }
    return S_OK;
  }
  
  IFACEMETHODIMP OnInteraction(
        IDirectManipulationViewport2* viewport,
        DIRECTMANIPULATION_INTERACTION_TYPE interaction) {
      return S_OK;
  }

private:
  Window *window_;
};

} // namespace fluxent
