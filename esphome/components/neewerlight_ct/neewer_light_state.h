#pragma once

#ifdef USE_ESP32

namespace esphome::neewerlight_ct {

enum class OnOffState {
  OFF = 0,
  ON = 1,
  UNKNOWN = 2,
};

// The only values we can change in the light are in this struct.
struct NeewerLightState {
  OnOffState on_off = OnOffState::UNKNOWN;
  float brightness = 0.0;         // 0.0 to 1.0 (0% to 100%)
  float color_temperature = 0.0;  // 0.0 to 1.0, normalized in mireds from coldest to warmest
};

}  // namespace esphome::neewerlight_ct

#endif  // USE_ESP32
