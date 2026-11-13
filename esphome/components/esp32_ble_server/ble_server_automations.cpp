#include "ble_server_automations.h"

#ifdef USE_ESP32

// Interface to interact with ESPHome automations and triggers
namespace esphome::esp32_ble_server::esp32_ble_server_automations {

using namespace esp32_ble;

#ifdef USE_ESP32_BLE_SERVER_SET_VALUE_ACTION
void BLECharacteristicSetValueActionManager::set_listener(BLECharacteristic *characteristic,
                                                          const std::function<void()> &pre_notify_listener) {
  // Find and remove existing listener for this characteristic
  auto *existing = this->find_listener_(characteristic);
  if (existing != nullptr) {
    // Remove from vector
    this->remove_listener_(characteristic);
  }
  // Save the entry to the vector
  this->listeners_.push_back({characteristic, pre_notify_listener});
}

BLECharacteristicSetValueActionManager::ListenerEntry *BLECharacteristicSetValueActionManager::find_listener_(
    BLECharacteristic *characteristic) {
  for (auto &entry : this->listeners_) {
    if (entry.characteristic == characteristic) {
      return &entry;
    }
  }
  return nullptr;
}

void BLECharacteristicSetValueActionManager::remove_listener_(BLECharacteristic *characteristic) {
  // Since we typically have very few listeners, optimize by swapping with back and popping
  for (size_t i = 0; i < this->listeners_.size(); i++) {
    if (this->listeners_[i].characteristic == characteristic) {
      // Swap with last element and pop (safe even when i is the last element)
      this->listeners_[i] = this->listeners_.back();
      this->listeners_.pop_back();
      return;
    }
  }
}
#endif

}  // namespace esphome::esp32_ble_server::esp32_ble_server_automations

#endif
