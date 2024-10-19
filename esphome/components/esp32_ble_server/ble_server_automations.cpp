#include "ble_server_automations.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {
// Interface to interact with ESPHome automations and triggers
namespace esp32_ble_server_automations {

using namespace esp32_ble;

Trigger<std::vector<uint8_t>> *BLETriggers::create_characteristic_on_write_trigger(BLECharacteristic *characteristic) {
  Trigger<std::vector<uint8_t>> *on_write_trigger =  // NOLINT(cppcoreguidelines-owning-memory)
      new Trigger<std::vector<uint8_t>>();
  characteristic->EventEmitter<BLECharacteristicEvt::VectorEvt, std::vector<uint8_t>>::on(
      BLECharacteristicEvt::VectorEvt::ON_WRITE,
      [on_write_trigger](const std::vector<uint8_t> &data) { on_write_trigger->trigger(data); });
  return on_write_trigger;
}

Trigger<std::vector<uint8_t>> *BLETriggers::create_descriptor_on_write_trigger(BLEDescriptor *descriptor) {
  Trigger<std::vector<uint8_t>> *on_write_trigger =  // NOLINT(cppcoreguidelines-owning-memory)
      new Trigger<std::vector<uint8_t>>();
  descriptor->EventEmitter<BLEDescriptorEvt::VectorEvt, std::vector<uint8_t>>::on(
      BLEDescriptorEvt::VectorEvt::ON_WRITE,
      [on_write_trigger](const std::vector<uint8_t> &data) { on_write_trigger->trigger(data); });
  return on_write_trigger;
}

void BLECharacteristicSetValueActionManager::set_listener(BLECharacteristic *characteristic,
                                                          EventEmitterListenerID listener_id,
                                                          const std::function<void()> &pre_notify_listener) {
  // Check if there is already a listener for this characteristic
  if (this->listeners_.count(characteristic) > 0) {
    // Unpack the pair listener_id, pre_notify_listener_id
    auto listener_pairs = this->listeners_[characteristic];
    EventEmitterListenerID old_listener_id = listener_pairs.first;
    EventEmitterListenerID old_pre_notify_listener_id = listener_pairs.second;
    // Remove the previous listener
    characteristic->EventEmitter<BLECharacteristicEvt::EmptyEvt>::off(BLECharacteristicEvt::EmptyEvt::ON_READ,
                                                                      old_listener_id);
    // Remove the pre-notify listener
    this->off(BLECharacteristicSetValueActionEvt::PRE_NOTIFY, old_pre_notify_listener_id);
  }
  // Create a new listener for the pre-notify event
  EventEmitterListenerID pre_notify_listener_id =
      this->on(BLECharacteristicSetValueActionEvt::PRE_NOTIFY,
               [pre_notify_listener, characteristic](const BLECharacteristic *evt_characteristic) {
                 // Only call the pre-notify listener if the characteristic is the one we are interested in
                 if (characteristic == evt_characteristic) {
                   pre_notify_listener();
                 }
               });
  // Save the pair listener_id, pre_notify_listener_id to the map
  this->listeners_[characteristic] = std::make_pair(listener_id, pre_notify_listener_id);
}

}  // namespace esp32_ble_server_automations
}  // namespace esp32_ble_server
}  // namespace esphome

#endif
