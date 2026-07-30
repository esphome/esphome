#pragma once
#ifdef USE_LIBRETINY
#include "ota_backend.h"

#include "esphome/core/defines.h"

namespace esphome::ota {

class ArduinoLibreTinyOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  // Unused: supports_sha256_checksum() is false, but the (non-virtual) backend
  // interface is shared across platforms, so this still needs to exist.
  void set_update_sha256(const char *sha256) {}
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }
  bool supports_sha256_checksum() { return false; }
  bool requires_sha256_checksum() { return false; }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoLibreTinyOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_LIBRETINY
