#include "light_color_values.h"

namespace esphome::light {

// Lightweight lerp without std::lerp's NaN/infinity handling overhead.
// Safe here because all color values are pre-clamped finite floats.
static inline float fast_lerp(float a, float b, float t) { return a + t * (b - a); }

LightColorValues LightColorValues::lerp(const LightColorValues &start, const LightColorValues &end, float completion) {
  // Directly interpolate the raw values to avoid getter/setter overhead.
  // This is safe because:
  // - All LightColorValues have their values clamped when set via the setters
  // - fast_lerp output stays in range when inputs are in range and 0 <= completion <= 1
  // - Therefore the output doesn't need clamping, so we can skip the setters
  LightColorValues v;
  v.color_mode_ = end.color_mode_;
  v.state_ = fast_lerp(start.state_, end.state_, completion);
  v.brightness_ = fast_lerp(start.brightness_, end.brightness_, completion);
  v.color_brightness_ = fast_lerp(start.color_brightness_, end.color_brightness_, completion);
  v.red_ = fast_lerp(start.red_, end.red_, completion);
  v.green_ = fast_lerp(start.green_, end.green_, completion);
  v.blue_ = fast_lerp(start.blue_, end.blue_, completion);
  v.white_ = fast_lerp(start.white_, end.white_, completion);
  v.color_temperature_ = fast_lerp(start.color_temperature_, end.color_temperature_, completion);
  v.cold_white_ = fast_lerp(start.cold_white_, end.cold_white_, completion);
  v.warm_white_ = fast_lerp(start.warm_white_, end.warm_white_, completion);
  return v;
}

}  // namespace esphome::light
