#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SENSOR

#include <string>

#include "esphome/components/sensor/sensor.h"

#include "ble_component_handler.h"

using std::string;

namespace esphome {
namespace esp32_ble_controller {

using sensor::Sensor;

/**
 * Special component handler for sensors, which adds the sensor's unit of measure to the component description.
 */
class BLESensorHandler : public BLEComponentHandler<Sensor> {
public:
  BLESensorHandler(Sensor* component, const BLECharacteristicInfoForHandler& characteristic_info) : BLEComponentHandler(component, characteristic_info) {}
  virtual ~BLESensorHandler() {}

protected:
  virtual string get_component_description();
};

} // namespace esp32_ble_controller
} // namespace esphome

#endif
