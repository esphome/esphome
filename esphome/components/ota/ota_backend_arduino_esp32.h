#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_LIBRETINY)

#include "ota_component.h"
#include "ota_backend.h"

namespace esphome {
namespace ota {

class ArduinoESP32OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }
  int get_backend_errno() override { return last_errno_; }

 private:
  int last_errno_;
};

}  // namespace ota
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO || USE_LIBRETINY
