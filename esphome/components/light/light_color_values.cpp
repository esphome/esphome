#include "light_color_values.h"

#include <cmath>

namespace esphome::light {

LightColorValues LightColorValues::lerp(const LightColorValues &start, const LightColorValues &end, float completion) {
  // Directly interpolate the raw values to avoid getter/setter overhead.
  // This is safe because:
  // - All LightColorValues have their values clamped when set via the setters
  // - std::lerp guarantees output is in the same range as inputs
  // - Therefore the output doesn't need clamping, so we can skip the setters
  LightColorValues v;
  v.color_mode_ = end.color_mode_;
  v.state_ = std::lerp(start.state_, end.state_, completion);
  v.brightness_ = std::lerp(start.brightness_, end.brightness_, completion);
  v.color_brightness_ = std::lerp(start.color_brightness_, end.color_brightness_, completion);
  v.red_ = std::lerp(start.red_, end.red_, completion);
  v.green_ = std::lerp(start.green_, end.green_, completion);
  v.blue_ = std::lerp(start.blue_, end.blue_, completion);
  v.white_ = std::lerp(start.white_, end.white_, completion);
  v.color_temperature_ = std::lerp(start.color_temperature_, end.color_temperature_, completion);
  v.cold_white_ = std::lerp(start.cold_white_, end.cold_white_, completion);
  v.warm_white_ = std::lerp(start.warm_white_, end.warm_white_, completion);
  return v;
}

}  // namespace esphome::light
