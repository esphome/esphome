#include "oralb_brush.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace oralb_brush {

static const char *TAG = "oralb_brush";

void OralbBrush::dump_config() {
  ESP_LOGCONFIG(TAG, "OralbBrush");
  LOG_SENSOR("  ", "State", this->state_);
}

}  // namespace oralb_brush
}  // namespace esphome

#endif
