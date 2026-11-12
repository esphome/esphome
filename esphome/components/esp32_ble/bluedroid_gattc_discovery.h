#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace esphome::esp32_ble {

/// Failed discovery statuses Bluedroid did not pass on, per connection, until the search result takes them.
/// Recorded on the Bluetooth host task and taken on the BTC task; a slot holds status << 16 | conn_id, 0 when free.
template<size_t N> class DiscoveryErrors {
 public:
  void record(uint16_t conn_id, uint8_t status) {
    const uint32_t entry = (uint32_t(status) << 16) | conn_id;
    for (auto &slot : this->slots_) {
      uint32_t current = slot.load(std::memory_order_relaxed);
      if ((current == 0 || uint16_t(current) == conn_id) && slot.compare_exchange_strong(current, entry))
        return;
    }
  }

  /// The recorded status for `conn_id`, cleared, or 0 when there is none.
  uint8_t take(uint16_t conn_id) {
    for (auto &slot : this->slots_) {
      uint32_t current = slot.load(std::memory_order_relaxed);
      if (current != 0 && uint16_t(current) == conn_id && slot.compare_exchange_strong(current, 0))
        return uint8_t(current >> 16);
    }
    return 0;
  }

 protected:
  std::array<std::atomic<uint32_t>, N> slots_{};
};

/// Bluedroid's own connection id, (tcb index << 8) | gatt_if, from the id and gattc_if an ESP_GATTC event
/// carries; the event's conn_id is the stack id >> 8 (BTC_GATT_CREATE_CONN_ID / BTC_GATT_GET_CONN_ID).
inline uint16_t stack_conn_id(uint16_t event_conn_id, uint8_t gattc_if) {
  return uint16_t(event_conn_id << 8) | gattc_if;
}

/// Take the failed discovery status recorded for Bluedroid's connection id; see bluedroid_gattc_discovery.cpp.
uint8_t take_discovery_error(uint16_t stack_conn_id);

}  // namespace esphome::esp32_ble
