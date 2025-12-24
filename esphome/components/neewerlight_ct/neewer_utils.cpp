#include "utils.h"

#include <cmath>

#ifdef USE_ESP32

namespace esphome::neewerlight_ct::utils {

float mireds_to_kelvin(float mireds) {
  if (mireds == 0.0f) {
    return 0.0f;
  }
  return 1e6f / mireds;
}

int mireds_to_kelvin_int(float mireds) { return std::round(mireds_to_kelvin(mireds)); }

float normalized_mireds_to_kelvin(float normalized_mireds, float coldest_mireds, float warmest_mireds) {
  // normalized color temperature must apply to mireds, it's not linear in Kelvin!
  float ct_mireds = coldest_mireds - normalized_mireds * (coldest_mireds - warmest_mireds);
  return mireds_to_kelvin(ct_mireds);
}

int normalized_mireds_to_kelvin_int(float normalized_mireds, float coldest_mireds, float warmest_mireds) {
  return std::round(normalized_mireds_to_kelvin(normalized_mireds, coldest_mireds, warmest_mireds));
}

int brightness_to_percent(float brightness) {
  if (brightness < 0.0f) {
    return 0;
  } else if (brightness > 1.0f) {
    return 100;
  } else {
    return std::round(brightness * 100.0f);
  }
}

uint8_t checksum(const std::vector<uint8_t> &data) {
  uint8_t checksum = 0;
  for (uint8_t byte : data) {
    checksum += byte;
  }
  return checksum;
}

}  // namespace esphome::neewerlight_ct::utils

#endif  // USE_ESP32
