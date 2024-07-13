#pragma once

#include "ble_characteristic.h"

#include "esphome/core/event_emitter.h"
#include "esphome/core/automation.h"

#include <vector>
#include <unordered_map>

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {
// Interface to interact with ESPHome actions and triggers
namespace esp32_ble_server_automations {

using namespace esp32_ble;

class BLETriggers {
 public:
  static Trigger<std::vector<uint8_t>> *create_on_write_trigger(BLECharacteristic *characteristic);
};

// Class to make sure only one BLECharacteristicSetValueAction is active at a time
class BLECharacteristicSetValueActionManager {
 public:
  // Singleton pattern
  static BLECharacteristicSetValueActionManager *get_instance() {
    static BLECharacteristicSetValueActionManager instance;
    return &instance;
  }
  void set_listener(BLECharacteristic *characteristic, EventEmitterListenerID listener_id);
  EventEmitterListenerID get_listener(BLECharacteristic *characteristic) { return this->listeners_[characteristic]; }

 private:
  std::unordered_map<BLECharacteristic *, EventEmitterListenerID> listeners_;
};

template<typename... Ts> class BLECharacteristicSetValueAction : public Action<Ts...> {
 public:
  BLECharacteristicSetValueAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, value)
  void play(Ts... x) override {
    // If the listener is already set, do nothing
    if (BLECharacteristicSetValueActionManager::get_instance()->get_listener(this->parent_) == this->listener_id_)
      return;
    // Set initial value
    this->parent_->set_value(this->value_.value(x...));
    // Set the listener for read events
    this->listener_id_ = this->parent_->EventEmitter<BLECharacteristicEvt::EmptyEvt>::on(
        BLECharacteristicEvt::EmptyEvt::ON_READ, [this, x...](void) {
          // Set the value of the characteristic every time it is read
          this->parent_->set_value(this->value_.value(x...));
        });
    // Set the listener in the global manager so only one BLECharacteristicSetValueAction is set for each characteristic
    BLECharacteristicSetValueActionManager::get_instance()->set_listener(this->parent_, this->listener_id_);
  }

 protected:
  BLECharacteristic *parent_;
  EventEmitterListenerID listener_id_;
};

template<typename... Ts> class BLECharacteristicNotifyAction : public Action<Ts...> {
 public:
  BLECharacteristicNotifyAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  void play(Ts... x) override { this->parent_->notify(); }

 protected:
  BLECharacteristic *parent_;
};

}  // namespace esp32_ble_server_automations
}  // namespace esp32_ble_server
}  // namespace esphome

#endif
