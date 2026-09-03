#include "light_color_values.h"

namespace esphome::light {

// Lightweight lerp: a + t * (b - a).
// Avoids std::lerp's NaN/infinity handling which Clang doesn't optimize out,
// adding ~200 bytes per call. Safe because all values are finite floats.
static float __attribute__((noinline)) lerp_fast(float a, float b, float t) { return a + t * (b - a); }

LightColorValues LightColorValues::lerp(const LightColorValues &start, const LightColorValues &end, float completion) {
  // Directly interpolate the raw values to avoid getter/setter overhead.
  // This is safe because:
  // - All LightColorValues except color_temperature_ have their values clamped when set via the setters
  // - lerp_fast output stays in range when inputs are in range and 0 <= completion <= 1
  // - Therefore the output doesn't need clamping, so we can skip the setters
  LightColorValues v;
  v.color_mode_ = end.color_mode_;
  v.state_ = lerp_fast(start.state_, end.state_, completion);
  v.brightness_ = lerp_fast(start.brightness_, end.brightness_, completion);
  v.color_brightness_ = lerp_fast(start.color_brightness_, end.color_brightness_, completion);
  v.red_ = lerp_fast(start.red_, end.red_, completion);
  v.green_ = lerp_fast(start.green_, end.green_, completion);
  v.blue_ = lerp_fast(start.blue_, end.blue_, completion);
  v.white_ = lerp_fast(start.white_, end.white_, completion);
  v.color_temperature_ = lerp_fast(start.color_temperature_, end.color_temperature_, completion);
  v.cold_white_ = lerp_fast(start.cold_white_, end.cold_white_, completion);
  v.warm_white_ = lerp_fast(start.warm_white_, end.warm_white_, completion);
  return v;
}

}  // namespace esphome::light
