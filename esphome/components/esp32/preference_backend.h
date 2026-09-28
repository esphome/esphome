#pragma once
#ifdef USE_ESP32

#include <cstddef>
#include <cstdint>

namespace esphome::esp32 {

class ESP32PreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t key{0};
  uint32_t nvs_handle{0};   // NVS (flash) path
  uint16_t rtc_offset{0};   // RTC path: word offset into the RTC storage region
  uint8_t length_words{0};  // RTC path: data length in 32-bit words
  bool in_flash{true};      // true: store in NVS (flash); false: store in RTC memory
};

class ESP32Preferences;
ESP32Preferences *get_preferences();

}  // namespace esphome::esp32

namespace esphome {
using PreferenceBackend = esp32::ESP32PreferenceBackend;
}  // namespace esphome

#endif  // USE_ESP32
