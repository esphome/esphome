#include "ble_fan_handler.h"

#ifdef USE_FAN

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_fan_handler";

void BLEFanHandler::on_characteristic_written() {
  std::string value = get_characteristic()->getValue();
  if (value.length() == 1) {
    uint8_t on = value[0];
    ESP_LOGD(TAG, "Fan chracteristic written: %d", on);
    if (on)
      get_component()->turn_on().perform();
    else
      get_component()->turn_off().perform();
  }
}

} // namespace esp32_ble_controller
} // namespace esphome

#endif
