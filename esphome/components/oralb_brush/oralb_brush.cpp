#include "oralb_brush.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace oralb_brush {

static const char *const TAG = "oralb_brush";

void OralbBrush::dump_config() {
  ESP_LOGCONFIG(TAG, "OralbBrush");
  LOG_SENSOR("  ", "State", this->state_);
}

bool OralbBrush::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  auto res = oralb_ble::parse_oralb(device);
  if (!res.has_value())
    return false;

  if (res->state.has_value() && this->state_ != nullptr)
    this->state_->publish_state(*res->state);

  return true;
}

}  // namespace oralb_brush
}  // namespace esphome

#endif  // USE_ESP32
