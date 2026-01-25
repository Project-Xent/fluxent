#pragma once

#include "fluxent/graphics.hpp"
#include "fluxent/config.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent::controls {

struct SliderLayout {
  Rect track_rect;
  Rect value_rect;
  Rect thumb_rect; // The touch target/visual thumb
  float thumb_center_x;
  float track_width;
  float track_start_x;
};

// Pure function to calculate slider geometry
inline SliderLayout CalculateSliderLayout(const Rect &bounds, float min,
                                          float max, float value,
                                          float scale = 1.0f) {
  SliderLayout layout;
  
  using namespace fluxent::config::Layout;

  float val = std::clamp(value, min, max);
  float range = max - min;
  float pct = (range > 0.0f) ? (val - min) / range : 0.0f;

  float center_y = bounds.y + bounds.height * 0.5f;
  
  // Thumb radius
  float outer_radius = SliderThumbSize * 0.5f;

  // Track Layout
  float track_left = bounds.x + outer_radius;
  float track_right = bounds.x + bounds.width - outer_radius;
  float track_w = (std::max)(0.0f, track_right - track_left);
  
  layout.track_width = track_w;
  layout.track_start_x = track_left; // New field
  
  // Thumb Position
  layout.thumb_center_x = track_left + pct * track_w;

  // Rects
  layout.track_rect = Rect(bounds.x, center_y - SliderTrackHeight * 0.5f, 
                           bounds.width, SliderTrackHeight);
                           
  layout.value_rect = Rect(bounds.x, center_y - SliderTrackHeight * 0.5f,
                           layout.thumb_center_x - bounds.x, SliderTrackHeight); // Width is diff

  // Visual thumb rect (centered)
  layout.thumb_rect = Rect(layout.thumb_center_x - outer_radius, 
                           center_y - outer_radius,
                           SliderThumbSize, SliderThumbSize);

  return layout;
}

struct CheckBoxLayout {
  Rect box_rect;
  Rect text_rect;
  float glyph_size;
};

inline CheckBoxLayout CalculateCheckBoxLayout(const Rect &bounds, float size, 
                                              float gap, float glyph_font_size) {
  CheckBoxLayout layout;
  
  float center_y = bounds.y + bounds.height * 0.5f;
  
  layout.box_rect = Rect(bounds.x, center_y - size * 0.5f, size, size);
  layout.glyph_size = glyph_font_size;
  
  layout.text_rect = bounds;
  layout.text_rect.x += size + gap;
  layout.text_rect.width = (std::max)(0.0f, layout.text_rect.width - (size + gap));
  
  return layout;
}

// ScrollBar Layout Logic
struct ScrollBarLayout {
  Rect bar_rect;
  Rect track_rect;
  Rect thumb_rect;
  bool visible = false;
};

struct ScrollViewLayout {
  ScrollBarLayout h_bar;
  ScrollBarLayout v_bar;
  float content_width;
  float content_height;
};

// To be used by both Input and Renderer
// Note: Requires passing determined content size and view size
inline ScrollViewLayout CalculateScrollViewLayout(
    float view_w, float view_h, 
    float content_w, float content_h,
    float scroll_x, float scroll_y,
    bool show_h, bool show_v) { 

  ScrollViewLayout layout;
  layout.content_width = content_w;
  layout.content_height = content_h;
  
  // Visibility is determined by caller (Separatoring Policy from Mechanism)

  using namespace fluxent::config::Layout;

  float effective_w = view_w;
  float effective_h = view_h;
  if (show_v) effective_w -= ScrollBarSize;
  if (show_h) effective_h -= ScrollBarSize;
  
  // Vertical
  layout.v_bar.visible = show_v;
  if (show_v) {
    layout.v_bar.bar_rect = Rect(view_w - ScrollBarSize, 0, ScrollBarSize, effective_h);
    layout.v_bar.track_rect = layout.v_bar.bar_rect;
    layout.v_bar.track_rect.y += ScrollBarPadding;
    layout.v_bar.track_rect.height = (std::max)(0.0f, layout.v_bar.track_rect.height - 2 * ScrollBarPadding);
    
    if (layout.v_bar.track_rect.height > 0) {
      float ratio = effective_h / content_h;
      float thumb_h = (std::max)(ScrollBarMinThumbSize, layout.v_bar.track_rect.height * ratio);
      float max_offset = content_h - effective_h;
      float scroll_ratio = max_offset > 0 ? scroll_y / max_offset : 0.0f;
      float track_scrollable = layout.v_bar.track_rect.height - thumb_h;
      
      layout.v_bar.thumb_rect = Rect(
          layout.v_bar.bar_rect.x + ScrollBarPadding,
          layout.v_bar.track_rect.y + scroll_ratio * track_scrollable,
          ScrollBarSize - 2 * ScrollBarPadding,
          thumb_h
      );
    }
  }
  
  // Horizontal
  layout.h_bar.visible = show_h;
  if (show_h) {
    layout.h_bar.bar_rect = Rect(0, view_h - ScrollBarSize, effective_w, ScrollBarSize);
    layout.h_bar.track_rect = layout.h_bar.bar_rect;
    layout.h_bar.track_rect.x += ScrollBarPadding;
    layout.h_bar.track_rect.width = (std::max)(0.0f, layout.h_bar.track_rect.width - 2 * ScrollBarPadding);
    
    if (layout.h_bar.track_rect.width > 0) {
      float ratio = effective_w / content_w;
      float thumb_w = (std::max)(ScrollBarMinThumbSize, layout.h_bar.track_rect.width * ratio);
      float max_offset = content_w - effective_w;
      float scroll_ratio = max_offset > 0 ? scroll_x / max_offset : 0.0f;
      float track_scrollable = layout.h_bar.track_rect.width - thumb_w;
      
      layout.h_bar.thumb_rect = Rect(
         layout.h_bar.track_rect.x + scroll_ratio * track_scrollable,
         layout.h_bar.bar_rect.y + ScrollBarPadding,
         thumb_w,
         ScrollBarSize - 2 * ScrollBarPadding
      );
    }
  }

  return layout;
}

} // namespace fluxent::controls
