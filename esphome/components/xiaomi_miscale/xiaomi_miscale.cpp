#include "xiaomi_miscale.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_miscale {

static const char *TAG = "xiaomi_miscale";

void XiaomiMiscale::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Mijia");
  LOG_SENSOR("  ", "Measured Weight", this->weight_);
}

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
