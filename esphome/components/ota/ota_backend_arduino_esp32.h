#include "esphome/core/defines.h"
#if USE_ARDUINO
#if USE_ESP32

#include "ota_component.h"
#include "ota_backend.h"

namespace esphome {
namespace ota {

class ArduinoESP32OTABackend: public OTABackend {
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
};

}  // namespace ota
}  // namespace esphome

#endif
#endif
