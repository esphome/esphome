#include "gatt_service_table_bluedroid.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT) && defined(USE_BLUEDROID_GATT_SERVICE_TABLE)

#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "gatt_service_table";

// A stack that never reports end-of-range would otherwise walk forever.
static constexpr uint16_t MAX_DESCRIPTORS_PER_CHARACTERISTIC = 64;

// Shared enumeration for both build passes: an identical walk order is what
// lets the counting pass size the block the filling pass fills.
// INVALID_OFFSET/NOT_FOUND mean end-of-range; anything else is a failure.
template<typename ServiceFn, typename CharFn, typename DescFn>
bool BluedroidServiceTable::walk_(ServiceFn &&on_service, CharFn &&on_char, DescFn &&on_desc) {
  for (uint16_t s = 0; s < this->service_total_; s++) {
    esp_gattc_service_elem_t svc;
    uint16_t svc_count = 1;
    auto svc_status = esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr, &svc, &svc_count, s);
    if (svc_status != ESP_GATT_OK || svc_count == 0) {
      this->log_walk_warning_("esp_ble_gattc_get_service", svc_status);
      return false;
    }
    if (!on_service(s, svc)) {
      return false;
    }
    uint16_t svc_chars = 0;
    auto count_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                                     svc.start_handle, svc.end_handle, 0, &svc_chars);
    if (count_status != ESP_GATT_OK) {
      this->log_walk_warning_("esp_ble_gattc_get_attr_count", count_status);
      return false;
    }
    for (uint16_t c = 0; c < svc_chars; c++) {
      esp_gattc_char_elem_t chr;
      uint16_t char_count = 1;
      auto status = esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, svc.start_handle, svc.end_handle, &chr,
                                               &char_count, c);
      if (status != ESP_GATT_OK || char_count == 0) {
        // An early terminator contradicts svc_chars from the same cache;
        // never build a silently truncated table.
        this->log_walk_warning_("esp_ble_gattc_get_all_char", status);
        return false;
      }
      if (!on_char(svc, chr)) {
        return false;
      }
      for (uint16_t d = 0;; d++) {
        if (d == MAX_DESCRIPTORS_PER_CHARACTERISTIC) {
          // A stack that never reports end-of-range; fail like every other
          // inconsistency instead of truncating the table silently.
          ESP_LOGW(TAG, "[%d] Descriptor walk exceeded %u entries", this->log_index_,
                   MAX_DESCRIPTORS_PER_CHARACTERISTIC);
          return false;
        }
        esp_gattc_descr_elem_t desc;
        uint16_t desc_count = 1;
        auto desc_status =
            esp_ble_gattc_get_all_descr(this->gattc_if_, this->conn_id_, chr.char_handle, &desc, &desc_count, d);
        if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (desc_status != ESP_GATT_OK || desc_count == 0) {
          this->log_walk_warning_("esp_ble_gattc_get_all_descr", desc_status);
          return false;
        }
        if (!on_desc(chr, desc)) {
          return false;
        }
      }
    }
  }
  return true;
}

bool BluedroidServiceTable::count_services(esp_gatt_if_t gattc_if, uint16_t conn_id, uint16_t *total) {
  uint16_t primary = 0;
  uint16_t secondary = 0;
  if (esp_ble_gattc_get_attr_count(gattc_if, conn_id, ESP_GATT_DB_PRIMARY_SERVICE, 0x0001, 0xFFFF, 0, &primary) !=
          ESP_GATT_OK ||
      esp_ble_gattc_get_attr_count(gattc_if, conn_id, ESP_GATT_DB_SECONDARY_SERVICE, 0x0001, 0xFFFF, 0, &secondary) !=
          ESP_GATT_OK) {
    // A failed count must not read as an authoritative empty database.
    return false;
  }
  *total = primary + secondary;
  return true;
}

bool BluedroidServiceTable::build(esp_gatt_if_t gattc_if, uint16_t conn_id, uint16_t service_total, uint8_t log_index) {
  this->free();
  this->gattc_if_ = gattc_if;
  this->conn_id_ = conn_id;
  this->service_total_ = service_total;
  this->log_index_ = log_index;

  // Pass 1: count, so one exact-size block holds the whole table.
  uint16_t char_total = 0;
  uint16_t desc_total = 0;
  bool counted = this->walk_([](uint16_t, const esp_gattc_service_elem_t &) { return true; },
                             [&](const esp_gattc_service_elem_t &, const esp_gattc_char_elem_t &) {
                               char_total++;
                               return true;
                             },
                             [&](const esp_gattc_char_elem_t &, const esp_gattc_descr_elem_t &) {
                               desc_total++;
                               return true;
                             });
  if (!counted) {
    ESP_LOGW(TAG, "[%d] Service table walk failed during count", this->log_index_);
    this->free();
    return false;
  }

  // The arrays share one block; carving stays aligned because each struct's
  // strictest member is the UUID and array sizes are multiples of it.
  static_assert(alignof(ble_device_base::GattService) >= alignof(ble_device_base::GattCharacteristic) &&
                alignof(ble_device_base::GattCharacteristic) >= alignof(ble_device_base::GattDescriptor));
  size_t svc_bytes = this->service_total_ * sizeof(ble_device_base::GattService);
  size_t char_bytes = char_total * sizeof(ble_device_base::GattCharacteristic);
  size_t total_bytes = svc_bytes + char_bytes + desc_total * sizeof(ble_device_base::GattDescriptor);
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  this->storage_ = allocator.allocate(total_bytes);
  if (this->storage_ == nullptr) {
    ESP_LOGW(TAG, "[%d] Service table allocation failed (%u bytes)", this->log_index_,
             static_cast<unsigned>(total_bytes));
    this->free();
    return false;
  }
  auto *services = reinterpret_cast<ble_device_base::GattService *>(this->storage_);
  auto *characteristics = reinterpret_cast<ble_device_base::GattCharacteristic *>(this->storage_ + svc_bytes);
  auto *descriptors = reinterpret_cast<ble_device_base::GattDescriptor *>(this->storage_ + svc_bytes + char_bytes);

  // Pass 2: fill, bounded by the pass-1 totals. A bound trip or a shortfall
  // means the cached database changed between the passes; fail the build
  // rather than serve an inconsistent table (the consumer retries).
  uint16_t char_index = 0;
  uint16_t desc_index = 0;
  ble_device_base::GattService *cur_service = nullptr;
  ble_device_base::GattCharacteristic *cur_char = nullptr;
  bool filled = this->walk_(
      [&](uint16_t s, const esp_gattc_service_elem_t &svc) {
        cur_service = &services[s];
        cur_service->uuid = ble_device_base::ESPBTUUID::from_uuid(svc.uuid);
        cur_service->start_handle = svc.start_handle;
        cur_service->end_handle = svc.end_handle;
        cur_service->first_characteristic = char_index;
        cur_service->characteristic_count = 0;
        return true;
      },
      [&](const esp_gattc_service_elem_t &svc, const esp_gattc_char_elem_t &chr) {
        if (char_index >= char_total) {
          return false;
        }
        cur_char = &characteristics[char_index++];
        cur_char->uuid = ble_device_base::ESPBTUUID::from_uuid(chr.uuid);
        cur_char->value_handle = chr.char_handle;
        // Bluedroid addresses descriptors by characteristic handle, so the
        // table's end_handle only needs the service-bounded upper bound.
        cur_char->end_handle = svc.end_handle;
        cur_char->properties = chr.properties;
        cur_char->first_descriptor = desc_index;
        cur_char->descriptor_count = 0;
        cur_service->characteristic_count++;
        return true;
      },
      [&](const esp_gattc_char_elem_t &, const esp_gattc_descr_elem_t &desc) {
        if (desc_index >= desc_total) {
          return false;
        }
        descriptors[desc_index].uuid = ble_device_base::ESPBTUUID::from_uuid(desc.uuid);
        descriptors[desc_index].handle = desc.handle;
        desc_index++;
        cur_char->descriptor_count++;
        return true;
      });
  if (!filled || char_index != char_total || desc_index != desc_total) {
    // Walk error or the database changed between passes; better an empty
    // table than a corrupt one.
    ESP_LOGW(TAG, "[%d] Service table walk mismatch, discarding", this->log_index_);
    this->free();
    return false;
  }
  this->char_total_ = char_total;
  this->desc_total_ = desc_total;
  return true;
}

void BluedroidServiceTable::log_walk_warning_(const char *operation, int code) {
  ESP_LOGW(TAG, "[%d] %s failed, status=%d", this->log_index_, operation, code);
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT && USE_BLUEDROID_GATT_SERVICE_TABLE
