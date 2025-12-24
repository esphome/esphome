#pragma once

#include <cstdint>
#include <vector>

#ifdef USE_ESP32

namespace esphome::neewerlight_ct::utils {

inline constexpr const char TAG[] = "neewerlight_ct.component";

// if input is 0, output is 0
float mireds_to_kelvin(float mireds);

int mireds_to_kelvin_int(float mireds);

// normalized_mireds is between 0.0 (coldest) and 1.0 (warmest)
float normalized_mireds_to_kelvin(float normalized_mireds, float coldest_mireds, float warmest_mireds);

int normalized_mireds_to_kelvin_int(float normalized_mireds, float coldest_mireds, float warmest_mireds);

int brightness_to_percent(float brightness);

uint8_t checksum(const std::vector<uint8_t> &data);

}  // namespace esphome::neewerlight_ct::utils

#endif  // USE_ESP32
