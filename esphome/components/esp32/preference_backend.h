#pragma once
#ifdef USE_ESP32

#include <cstddef>
#include <cstdint>

namespace esphome::esp32 {

class ESP32PreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t key;
  uint32_t nvs_handle;
};

class ESP32Preferences;
ESP32Preferences *get_preferences();

}  // namespace esphome::esp32

namespace esphome {
using PreferenceBackend = esp32::ESP32PreferenceBackend;
}  // namespace esphome

#endif  // USE_ESP32
