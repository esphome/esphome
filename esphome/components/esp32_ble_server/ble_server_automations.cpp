#include "ble_server_automations.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {
// Interface to interact with ESPHome automations and triggers
namespace esp32_ble_server_automations {

using namespace esp32_ble;

Trigger<std::vector<uint8_t>> *BLETriggers::create_on_write_trigger(BLECharacteristic *characteristic) {
  Trigger<std::vector<uint8_t>> *on_write_trigger = new Trigger<std::vector<uint8_t>>();  // NOLINT(cppcoreguidelines-owning-memory)
  characteristic->EventEmitter<BLECharacteristicEvt::VectorEvt, std::vector<uint8_t>>::on(BLECharacteristicEvt::VectorEvt::ON_WRITE, [on_write_trigger](const std::vector<uint8_t> &data) {
    on_write_trigger->trigger(data);
  });
  return on_write_trigger;
}

void BLECharacteristicSetValueActionManager::set_listener(BLECharacteristic *characteristic, EventEmitterListenerID listener_id) { 
  // Check if there is already a listener for this characteristic
  if (this->listeners_.find(characteristic) != this->listeners_.end()) {
    // Remove the previous listener
    characteristic->EventEmitter<BLECharacteristicEvt::EmptyEvt>::off(BLECharacteristicEvt::EmptyEvt::ON_READ, this->listeners_[characteristic]);
  }
  this->listeners_[characteristic] = listener_id;
}

}  // namespace esp32_ble_server_automations
}  // namespace esp32_ble_server
}  // namespace esphome

#endif
