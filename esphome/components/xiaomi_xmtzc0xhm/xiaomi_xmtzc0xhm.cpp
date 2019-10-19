#include "xiaomi_xmtzc0xhm.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm {

static const char *TAG = "xiaomi_xmtzc0xhm";

void XiaomiMiscale::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Mijia");
  LOG_SENSOR("  ", "Measured Weight", this->weight_);
}

}  // namespace xiaomi_xmtzc0xhm
}  // namespace esphome

#endif
