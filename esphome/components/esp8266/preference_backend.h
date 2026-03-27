#pragma once
#ifdef USE_ESP8266

#include <cstddef>
#include <cstdint>

namespace esphome::esp8266 {

class ESP8266PreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t type = 0;
  uint16_t offset = 0;
  uint8_t length_words = 0;  // Max 255 words (1020 bytes of data)
  bool in_flash = false;
};

class ESP8266Preferences;
ESP8266Preferences *get_preferences();

}  // namespace esphome::esp8266

namespace esphome {
using PreferenceBackend = esp8266::ESP8266PreferenceBackend;
}  // namespace esphome

#endif  // USE_ESP8266
