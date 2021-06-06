#pragma once

#include "ble_component_handler_base.h"

using std::string;

namespace esphome {
namespace esp32_ble_controller {

/**
 * A component handler controls a single component (sensor, switch, ...). 
 * Each component corresponds one-to-one to a BLE characteristic. When the state of the component changes in ESPHome, this handler updates the characteristic 
 * so that a BLE client gets notified about the change and can read the new value.
 * Some components like switch can be manipulated by the BLE client, i.e. the client writes a new value to the characteristic, which this handler observes and 
 * performs the required changes like turning on the switch.
 * <para>
 * Note: This is a type-safe variant of its base class.
 * @brief Controls a single component (sensor, switch, ...), propagates state changes to the BLE client and executes change request from the client.
 */
template <typename C>
class BLEComponentHandler : public BLEComponentHandlerBase {
public:
  BLEComponentHandler(C* component, const BLECharacteristicInfoForHandler& characteristic_info) : BLEComponentHandlerBase(component, characteristic_info) {}
  virtual ~BLEComponentHandler() {}

protected:
  virtual C* get_component() override { return static_cast<C*>(BLEComponentHandlerBase::get_component()); }
  
};

} // namespace esp32_ble_controller
} // namespace esphome
