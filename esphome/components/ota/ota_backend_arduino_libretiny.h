#pragma once
#include "esphome/core/defines.h"
#ifdef USE_LIBRETINY

#include "ota_component.h"
#include "ota_backend.h"

namespace esphome {
namespace ota {

class ArduinoLibreTinyOTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }
};

}  // namespace ota
}  // namespace esphome

#endif  // USE_LIBRETINY
