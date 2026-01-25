#pragma once

#include "fluxent/controls/renderer_utils.hpp" // Changed path to include prefix
#include <chrono>
#include <cmath>
#include <algorithm> // missing in previous write

namespace fluxent::controls {

// Generic Animator for simple transitions (float, Color, etc.)
template <typename T> struct Animator {
  T current;
  T target;
  T start_val;
  std::chrono::steady_clock::time_point start_time;
  bool active = false;
  bool initialized = false;



  // Update the animation state. Returns true if animation is active.
  bool Update(T new_target, double duration_seconds, T* out_value) {
    auto now = std::chrono::steady_clock::now();

    if (!initialized) {
      current = new_target;
      target = new_target;
      start_val = new_target;
      active = false;
      initialized = true;
      *out_value = current;
      return false;
    }

    // Check if target changed
    bool changed = false;
    // Simple equality check
    if constexpr (std::is_same_v<T, float>) {
        if (std::abs(new_target - target) > 0.001f) changed = true;
    } else {
         // Assuming Color or others have != or we just update always if diff
         // For Color we can use helper
         if (!ValuesEqual(target, new_target)) changed = true;
    }
    
    if (changed) {
      start_val = current;
      target = new_target;
      start_time = now;
      active = true;
    }

    if (active) {
      std::chrono::duration<double> elapsed = now - start_time;
      double t_d = elapsed.count() / duration_seconds;
      float t = static_cast<float>(std::clamp(t_d, 0.0, 1.0));

      if constexpr (std::is_same_v<T, fluxent::Color>) {
           current = LerpColorSrgb(start_val, target, t);
      } else {
           current = start_val + (target - start_val) * t;
      }

      if (t >= 1.0f) {
        current = target;
        active = false;
      }
    }
    
    *out_value = current;
    return active;
  }
  
  bool ValuesEqual(const float& a, const float& b) {
      return std::abs(a - b) < 0.001f;
  }
  
  bool ValuesEqual(const Color& a, const Color& b) {
      return ColorEqualRgba(a, b);
  }
  
  template<typename U>
  bool ValuesEqual(const U& a, const U& b) { return a == b; }
};

} // namespace fluxent::controls
