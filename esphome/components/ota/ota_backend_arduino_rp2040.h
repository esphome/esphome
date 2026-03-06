#pragma once
#ifdef USE_ARDUINO
#ifdef USE_RP2040
#include "ota_backend.h"

#include "esphome/core/defines.h"
#include "esphome/core/macros.h"

namespace esphome {
namespace ota {

class ArduinoRP2040OTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 private:
  bool md5_set_{false};
};

std::unique_ptr<ArduinoRP2040OTABackend> make_ota_backend();

}  // namespace ota
}  // namespace esphome

#endif  // USE_RP2040
#endif  // USE_ARDUINO
