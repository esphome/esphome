#pragma once
#ifdef USE_LIBRETINY
#include "ota_backend.h"

#include "esphome/core/defines.h"

namespace esphome::ota {

class ArduinoLibreTinyOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoLibreTinyOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_LIBRETINY
