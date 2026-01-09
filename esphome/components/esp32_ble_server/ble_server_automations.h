#pragma once

#include "ble_server.h"
#include "ble_characteristic.h"
#include "ble_descriptor.h"

#include "esphome/core/automation.h"

#include <vector>
#include <functional>

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {
// Interface to interact with ESPHome actions and triggers
namespace esp32_ble_server_automations {

using namespace esp32_ble;

class BLETriggers {
 public:
#ifdef USE_ESP32_BLE_SERVER_CHARACTERISTIC_ON_WRITE
  static Trigger<std::vector<uint8_t>, uint16_t> *create_characteristic_on_write_trigger(
      BLECharacteristic *characteristic);
#endif
#ifdef USE_ESP32_BLE_SERVER_DESCRIPTOR_ON_WRITE
  static Trigger<std::vector<uint8_t>, uint16_t> *create_descriptor_on_write_trigger(BLEDescriptor *descriptor);
#endif
#ifdef USE_ESP32_BLE_SERVER_ON_CONNECT
  static Trigger<uint16_t> *create_server_on_connect_trigger(BLEServer *server);
#endif
#ifdef USE_ESP32_BLE_SERVER_ON_DISCONNECT
  static Trigger<uint16_t> *create_server_on_disconnect_trigger(BLEServer *server);
#endif
};

#ifdef USE_ESP32_BLE_SERVER_SET_VALUE_ACTION
// Class to make sure only one BLECharacteristicSetValueAction is active at a time for each characteristic
class BLECharacteristicSetValueActionManager {
 public:
  // Singleton pattern
  static BLECharacteristicSetValueActionManager *get_instance() {
    static BLECharacteristicSetValueActionManager instance;
    return &instance;
  }
  void set_listener(BLECharacteristic *characteristic, const std::function<void()> &pre_notify_listener);
  bool has_listener(BLECharacteristic *characteristic) { return this->find_listener_(characteristic) != nullptr; }
  void emit_pre_notify(BLECharacteristic *characteristic) {
    for (const auto &entry : this->listeners_) {
      if (entry.characteristic == characteristic) {
        entry.pre_notify_listener();
        break;
      }
    }
  }

 private:
  struct ListenerEntry {
    BLECharacteristic *characteristic;
    std::function<void()> pre_notify_listener;
  };
  std::vector<ListenerEntry> listeners_;

  ListenerEntry *find_listener_(BLECharacteristic *characteristic);
  void remove_listener_(BLECharacteristic *characteristic);
};

template<typename... Ts> class BLECharacteristicSetValueAction : public Action<Ts...> {
 public:
  BLECharacteristicSetValueAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, buffer)
  void set_buffer(ByteBuffer buffer) { this->set_buffer(buffer.get_data()); }
  void play(const Ts &...x) override {
    // If the listener is already set, do nothing
    if (BLECharacteristicSetValueActionManager::get_instance()->has_listener(this->parent_))
      return;
    // Set initial value
    this->parent_->set_value(this->buffer_.value(x...));
    // Set the listener for read events
    this->parent_->on_read([this, x...](uint16_t id) {
      // Set the value of the characteristic every time it is read
      this->parent_->set_value(this->buffer_.value(x...));
    });
    // Set the listener in the global manager so only one BLECharacteristicSetValueAction is set for each characteristic
    BLECharacteristicSetValueActionManager::get_instance()->set_listener(
        this->parent_, [this, x...]() { this->parent_->set_value(this->buffer_.value(x...)); });
  }

 protected:
  BLECharacteristic *parent_;
};
#endif  // USE_ESP32_BLE_SERVER_SET_VALUE_ACTION

#ifdef USE_ESP32_BLE_SERVER_NOTIFY_ACTION
template<typename... Ts> class BLECharacteristicNotifyAction : public Action<Ts...> {
 public:
  BLECharacteristicNotifyAction(BLECharacteristic *characteristic) : parent_(characteristic) {}
  void play(const Ts &...x) override {
#ifdef USE_ESP32_BLE_SERVER_SET_VALUE_ACTION
    // Call the pre-notify event
    BLECharacteristicSetValueActionManager::get_instance()->emit_pre_notify(this->parent_);
#endif
    // Notify the characteristic
    this->parent_->notify();
  }

 protected:
  BLECharacteristic *parent_;
};
#endif  // USE_ESP32_BLE_SERVER_NOTIFY_ACTION

#ifdef USE_ESP32_BLE_SERVER_DESCRIPTOR_SET_VALUE_ACTION
template<typename... Ts> class BLEDescriptorSetValueAction : public Action<Ts...> {
 public:
  BLEDescriptorSetValueAction(BLEDescriptor *descriptor) : parent_(descriptor) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, buffer)
  void set_buffer(ByteBuffer buffer) { this->set_buffer(buffer.get_data()); }
  void play(const Ts &...x) override { this->parent_->set_value(this->buffer_.value(x...)); }

 protected:
  BLEDescriptor *parent_;
};
#endif  // USE_ESP32_BLE_SERVER_DESCRIPTOR_SET_VALUE_ACTION

}  // namespace esp32_ble_server_automations
}  // namespace esp32_ble_server
}  // namespace esphome

#endif
