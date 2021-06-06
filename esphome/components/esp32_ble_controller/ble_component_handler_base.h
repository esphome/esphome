#pragma once

#include <string>

#include <BLEServer.h>
#include <BLECharacteristic.h>

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"

using std::string;

namespace esphome {
namespace esp32_ble_controller {

struct BLECharacteristicInfoForHandler {
  string service_UUID;
  string characteristic_UUID;
  boolean use_BLE2902;
};

/**
 * A component handler controls a single component (sensor, switch, ...). 
 * Each component corresponds one-to-one to a BLE characteristic. When the state of the component changes in ESPHome, this handler updates the characteristic 
 * so that a BLE client gets notified about the change and can read the new value.
 * Some components like switch can be manipulated by the BLE client, i.e. the client writes a new value to the characteristic, which this handler observes and 
 * performs the required changes like turning on the switch.
 * @brief Controls a single component (sensor, switch, ...), propagates state changes to the BLE client and executes change request from the client.
 */
class BLEComponentHandlerBase : private BLECharacteristicCallbacks {
public:
  BLEComponentHandlerBase(Nameable* component, const BLECharacteristicInfoForHandler& characteristic_info);
  virtual ~BLEComponentHandlerBase();

  void setup(BLEServer* ble_server);

  void send_value(float value);
  void send_value(string value);
  void send_value(bool value);

protected:
  virtual Nameable* get_component() { return component; }
  virtual string get_component_description() { return get_component()->get_name(); }
  BLECharacteristic* get_characteristic() { return characteristic; }

  virtual bool can_receive_writes() { return false; }
  virtual void on_characteristic_written() {}

  bool is_security_enabled();
  
private:
  virtual void onWrite(BLECharacteristic *characteristic); // inherited from BLECharacteristicCallbacks

  Nameable* component;
  BLECharacteristicInfoForHandler characteristic_info;

  BLECharacteristic* characteristic;
};

} // namespace esp32_ble_controller
} // namespace esphome
