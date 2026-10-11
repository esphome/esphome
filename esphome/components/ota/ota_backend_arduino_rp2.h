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
  // Unused: supports_sha256_checksum() is false, but the (non-virtual) backend
  // interface is shared across platforms, so this still needs to exist.
  void set_update_sha256(const char *sha256) {}
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  // The core's OTA stub inflates a staged gzip image at reboot, on every chip
  // from 4.0.3 (ESPHome pins 6.0.0). begin() only sees the gzip size; the
  // inflated size is known when the stub reads the trailer.
  static constexpr bool supports_compression() { return USE_ARDUINO_VERSION_CODE >= VERSION_CODE(4, 0, 3); }
  static constexpr bool supports_sha256_checksum() { return false; }
  static constexpr bool requires_sha256_checksum() { return false; }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoRP2OTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_RP2
#endif  // USE_ARDUINO
