#pragma once
#ifdef USE_ARDUINO
#ifdef USE_RP2
#include "ota_backend.h"

#include "esphome/core/defines.h"
#include "esphome/core/macros.h"

namespace esphome::ota {

class ArduinoRP2OTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  // A gzip image is staged on LittleFS as is; the core's OTA stub inflates it
  // into the app region at reboot (arduino-pico 2.4.0 and later, RP2350 from
  // 4.0.3; ESPHome requires 6.0.0), the same way the ESP8266 bootloader does.
  // begin() then sees the gzip size, so only the staging space is checked up
  // front; the inflated size is not known until the stub reads the trailer.
  static constexpr bool supports_compression() { return true; }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoRP2OTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_RP2
#endif  // USE_ARDUINO
