#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SWITCH

#include <string>

#include "esphome/components/switch/switch.h"

#include "ble_component_handler.h"

using std::string;

namespace esphome {
namespace esp32_ble_controller {

using switch_::Switch;

/**
 * Special component handler for switches, which allows turning the switch on and off from a BLE client.
 */
class BLESwitchHandler : public BLEComponentHandler<Switch> {
public:
  BLESwitchHandler(Switch* component, const BLECharacteristicInfoForHandler& characteristic_info) : BLEComponentHandler(component, characteristic_info) {}
  virtual ~BLESwitchHandler() {}

protected:
  virtual bool can_receive_writes() { return true; }
  virtual void on_characteristic_written() override;
};

} // namespace esp32_ble_controller
} // namespace esphome

#endif