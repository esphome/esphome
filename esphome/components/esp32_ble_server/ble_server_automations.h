#pragma once

#include "ble_server.h"
#include "ble_characteristic.h"
#include "ble_descriptor.h"

#include "esphome/components/event_emitter/event_emitter.h"
#include "esphome/core/automation.h"

#include <vector>
#include <unordered_map>
#include <functional>

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {
// Interface to interact with ESPHome actions and triggers
namespace esp32_ble_server_automations {

using namespace esp32_ble;
using namespace event_emitter;

class BLETriggers {
 public:
  static Trigger<std::vector<uint8_t>, uint16_t> *create_characteristic_on_write_trigger(
      BLECharacteristic *characteristic);
  static Trigger<std::vector<uint8_t>, uint16_t> *create_descriptor_on_write_trigger(BLEDescriptor *descriptor);
  static Trigger<uint16_t> *create_server_on_connect_trigger(BLEServer *server);
  static Trigger<uint16_t> *create_server_on_disconnect_trigger(BLEServer *server);
};

enum BLECharacteristicSetValueActionEvt {
  PRE_NOTIFY,
};

// Class to make sure only one BLECharacteristicSetValueAction is active at a time for each characteristic
class BLECharacteristicSetValueActionManager
    : public EventEmitter<BLECharacteristicSetValueActionEvt, BLECharacteristic *> {
 public:
  // Singleton pattern
  static BLECharacteristicSetValueActionManager *get_instance() {
    static BLECharacteristicSetValueActionManager instance;
    return &instance;
  }
  void set_listener(BLECharacteristic *characteristic, EventEmitterListenerID listener_id,
                    const std::function<void()> &pre_notify_listener);
  EventEmitterListenerID get_listener(BLECharacteristic *characteristic) {
    return this->listeners_[characteristic].first;
  }
  void emit_pre_notify(BLECharacteristic *characteristic) {
    this->emit_(BLECharacteristicSetValueActionEvt::PRE_NOTIFY, characteristic);
  }

 private:
  std::unordered_map<BLECharacteristic *, std::pair<EventEmitterListenerID, EventEmitterListenerID>> listeners_;
};

template<typename... Ts> class BLECharacteristicSetValueAction : public Action<Ts...> {
 public:
  BLECharacteristicSetValueAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, buffer)
  void set_buffer(ByteBuffer buffer) { this->set_buffer(buffer.get_data()); }
  void play(Ts... x) override {
    // If the listener is already set, do nothing
    if (BLECharacteristicSetValueActionManager::get_instance()->get_listener(this->parent_) == this->listener_id_)
      return;
    // Set initial value
    this->parent_->set_value(this->buffer_.value(x...));
    // Set the listener for read events
    this->listener_id_ = this->parent_->EventEmitter<BLECharacteristicEvt::EmptyEvt, uint16_t>::on(
        BLECharacteristicEvt::EmptyEvt::ON_READ, [this, x...](uint16_t id) {
          // Set the value of the characteristic every time it is read
          this->parent_->set_value(this->buffer_.value(x...));
        });
    // Set the listener in the global manager so only one BLECharacteristicSetValueAction is set for each characteristic
    BLECharacteristicSetValueActionManager::get_instance()->set_listener(
        this->parent_, this->listener_id_, [this, x...]() { this->parent_->set_value(this->buffer_.value(x...)); });
  }

 protected:
  BLECharacteristic *parent_;
  EventEmitterListenerID listener_id_;
};

template<typename... Ts> class BLECharacteristicNotifyAction : public Action<Ts...> {
 public:
  BLECharacteristicNotifyAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  void play(Ts... x) override {
    // Call the pre-notify event
    BLECharacteristicSetValueActionManager::get_instance()->emit_pre_notify(this->parent_);
    // Notify the characteristic
    this->parent_->notify();
  }

 protected:
  BLECharacteristic *parent_;
};

template<typename... Ts> class BLEDescriptorSetValueAction : public Action<Ts...> {
 public:
  BLEDescriptorSetValueAction(BLEDescriptor *descriptor) : parent_(descriptor) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, buffer)
  void set_buffer(ByteBuffer buffer) { this->set_buffer(buffer.get_data()); }
  void play(Ts... x) override { this->parent_->set_value(this->buffer_.value(x...)); }

 protected:
  BLEDescriptor *parent_;
};

}  // namespace esp32_ble_server_automations
}  // namespace esp32_ble_server
}  // namespace esphome

#endif
