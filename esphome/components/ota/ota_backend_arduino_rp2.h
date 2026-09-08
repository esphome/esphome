#pragma once
#ifdef USE_ARDUINO
#ifdef USE_RP2
#include "ota_backend.h"

#include "esphome/core/defines.h"
#include "esphome/core/macros.h"

#include <RP2040Version.h>

namespace esphome::ota {

class ArduinoRP2OTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  // The core's OTA stub inflates a staged gzip image at reboot, on every chip
  // from 4.0.3 (ESPHome pins 6.0.0). begin() only sees the gzip size; the
  // inflated size is known when the stub reads the trailer.
  static constexpr bool supports_compression() {
    return VERSION_CODE(ARDUINO_PICO_MAJOR, ARDUINO_PICO_MINOR, ARDUINO_PICO_REVISION) >= VERSION_CODE(4, 0, 3);
  }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoRP2OTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_RP2
#endif  // USE_ARDUINO
