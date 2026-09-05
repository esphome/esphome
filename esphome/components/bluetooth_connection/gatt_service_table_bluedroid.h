// Owning two-pass materializer of one Bluedroid GATT database snapshot into
// the neutral GattServiceTable layout, shared by the BluedroidGattClient
// backend and ble_client's esp32 engine.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT) && defined(USE_BLUEDROID_GATT_SERVICE_TABLE)

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/core/helpers.h"

#include <esp_gattc_api.h>

namespace esphome::bluetooth_connection {

class BluedroidServiceTable {
 public:
  ~BluedroidServiceTable() { this->free(); }
  // Owns storage_; a copy would double-free.
  BluedroidServiceTable() = default;
  BluedroidServiceTable(const BluedroidServiceTable &) = delete;
  BluedroidServiceTable &operator=(const BluedroidServiceTable &) = delete;

  /// The service count build() requires: the stack's PRIMARY+SECONDARY
  /// attribute totals, never the SEARCH_RES event count.
  static bool count_services(esp_gatt_if_t gattc_if, uint16_t conn_id, uint16_t *total);

  /// Two-pass build from the stack's cached database (service_total from
  /// count_services()). log_index labels warnings. Frees any previous table
  /// first; on failure the table is left empty.
  bool build(esp_gatt_if_t gattc_if, uint16_t conn_id, uint16_t service_total, uint8_t log_index);

  // The view is carved from the storage block and the counts on each call
  // (a cold path) rather than cached, saving a per-instance table member.
  ble_device_base::GattServiceTable view() const {
    size_t svc_bytes = this->service_total_ * sizeof(ble_device_base::GattService);
    size_t char_bytes = this->char_total_ * sizeof(ble_device_base::GattCharacteristic);
    return {reinterpret_cast<const ble_device_base::GattService *>(this->storage_),
            reinterpret_cast<const ble_device_base::GattCharacteristic *>(this->storage_ + svc_bytes),
            reinterpret_cast<const ble_device_base::GattDescriptor *>(this->storage_ + svc_bytes + char_bytes),
            this->service_total_,
            this->char_total_,
            this->desc_total_};
  }

  // Always resets the counts: a failed build must never leave a non-zero
  // service_total_ behind a null table.
  void free() {
    if (this->storage_ != nullptr) {
      RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
      allocator.deallocate(this->storage_, 0);
      this->storage_ = nullptr;
    }
    this->service_total_ = 0;
    this->char_total_ = 0;
    this->desc_total_ = 0;
  }

  bool empty() const { return this->storage_ == nullptr; }

 private:
  template<typename ServiceFn, typename CharFn, typename DescFn>
  bool walk_(ServiceFn &&on_service, CharFn &&on_char, DescFn &&on_desc);
  void log_walk_warning_(const char *operation, int code);

  uint8_t *storage_{nullptr};
  uint16_t service_total_{0};
  uint16_t char_total_{0};
  uint16_t desc_total_{0};
  // Walk context, set by build().
  uint16_t conn_id_{0};
  esp_gatt_if_t gattc_if_{};  // uint8_t width
  uint8_t log_index_{0};
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT && USE_BLUEDROID_GATT_SERVICE_TABLE
