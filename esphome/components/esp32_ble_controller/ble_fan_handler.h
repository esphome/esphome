#pragma once

#include "esphome/core/defines.h"
#ifdef USE_FAN

#include <string>

#include "esphome/components/fan/fan_state.h"

#include "ble_component_handler.h"

using std::string;

namespace esphome {
namespace esp32_ble_controller {

using fan::FanState;

/**
 * Special component handler for fans, which allows turning the fan on and off from a BLE client.
 */
class BLEFanHandler : public BLEComponentHandler<FanState> {
public:
  BLEFanHandler(FanState* component, const BLECharacteristicInfoForHandler& characteristic_info) : BLEComponentHandler(component, characteristic_info) {}
  virtual ~BLEFanHandler() {}

protected:
  virtual bool can_receive_writes() { return true; }
  virtual void on_characteristic_written() override;
};

} // namespace esp32_ble_controller
} // namespace esphome

#endif
