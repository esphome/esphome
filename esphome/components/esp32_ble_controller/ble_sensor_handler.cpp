#include "ble_sensor_handler.h"

#ifdef USE_SENSOR

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_sensor_handler";

string BLESensorHandler::get_component_description() {
  string uom = get_component()->get_unit_of_measurement();
  if (uom.empty()) {
    return BLEComponentHandler::get_component_description();
  } else {
    return BLEComponentHandler::get_component_description() + " (" + uom + ")";
  }
}

} // namespace esp32_ble_controller
} // namespace esphome

#endif