#pragma once

#ifdef USE_ESP32

#include <cstddef>  // for offsetof
#include <vector>

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

#include "ble_scan_result.h"

namespace esphome {
namespace esp32_ble {

// Compile-time verification that ESP-IDF scan complete events only contain a status field
// This ensures our reinterpret_cast in ble.cpp is safe
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF scan_param_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF scan_start_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF scan_stop_cmpl structure has unexpected size");

// Verify the status field is at offset 0 (first member)
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_param_cmpl.status) ==
                  offsetof(esp_ble_gap_cb_param_t, scan_param_cmpl),
              "status must be first member of scan_param_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_start_cmpl.status) ==
                  offsetof(esp_ble_gap_cb_param_t, scan_start_cmpl),
              "status must be first member of scan_start_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_stop_cmpl.status) ==
                  offsetof(esp_ble_gap_cb_param_t, scan_stop_cmpl),
              "status must be first member of scan_stop_cmpl");

// Received GAP, GATTC and GATTS events are only queued, and get processed in the main loop().
// This class stores each event with minimal memory usage.
// GAP events (99% of traffic) don't have the vector overhead.
// GATTC/GATTS events use heap allocation for their param and data.
//
// Event flow:
// 1. ESP-IDF BLE stack calls our static handlers in the BLE task context
// 2. The handlers create a BLEEvent instance, copying only the data we need
// 3. The event is pushed to a thread-safe queue
// 4. In the main loop(), events are popped from the queue and processed
// 5. The event destructor cleans up any external allocations
//
// Thread safety:
// - GAP events: We copy only the fields we need directly into the union
// - GATTC/GATTS events: We heap-allocate and copy the entire param struct, ensuring
//   the data remains valid even after the BLE callback returns. The original
//   param pointer from ESP-IDF is only valid during the callback.
class BLEEvent {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum ble_event_t : uint8_t {
    GAP,
    GATTC,
    GATTS,
  };

  // Constructor for GAP events - no external allocations needed
  BLEEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->type_ = GAP;
    this->event_.gap.gap_event = e;

    if (p == nullptr) {
      return;  // Invalid event, but we can't log in header file
    }

    // Only copy the data we actually use for each GAP event type
    switch (e) {
      case ESP_GAP_BLE_SCAN_RESULT_EVT:
        // Copy only the fields we use from scan results
        memcpy(this->event_.gap.scan_result.bda, p->scan_rst.bda, sizeof(esp_bd_addr_t));
        this->event_.gap.scan_result.ble_addr_type = p->scan_rst.ble_addr_type;
        this->event_.gap.scan_result.rssi = p->scan_rst.rssi;
        this->event_.gap.scan_result.adv_data_len = p->scan_rst.adv_data_len;
        this->event_.gap.scan_result.scan_rsp_len = p->scan_rst.scan_rsp_len;
        this->event_.gap.scan_result.search_evt = p->scan_rst.search_evt;
        memcpy(this->event_.gap.scan_result.ble_adv, p->scan_rst.ble_adv,
               ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX);
        break;

      case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_param_cmpl.status;
        break;

      case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_start_cmpl.status;
        break;

      case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_stop_cmpl.status;
        break;

      default:
        // We only handle 4 GAP event types, others are dropped
        break;
    }
  }

  // Constructor for GATTC events - uses heap allocation
  // Creates a copy of the param struct since the original is only valid during the callback
  BLEEvent(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    this->type_ = GATTC;
    this->event_.gattc.gattc_event = e;
    this->event_.gattc.gattc_if = i;

    if (p == nullptr) {
      this->event_.gattc.gattc_param = nullptr;
      this->event_.gattc.data = nullptr;
      return;  // Invalid event, but we can't log in header file
    }

    // Heap-allocate param and data
    // Heap allocation is used because GATTC/GATTS events are rare (<1% of events)
    // while GAP events (99%) are stored inline to minimize memory usage
    this->event_.gattc.gattc_param = new esp_ble_gattc_cb_param_t(*p);

    // Copy data for events that need it
    switch (e) {
      case ESP_GATTC_NOTIFY_EVT:
        this->event_.gattc.data = new std::vector<uint8_t>(p->notify.value, p->notify.value + p->notify.value_len);
        this->event_.gattc.gattc_param->notify.value = this->event_.gattc.data->data();
        break;
      case ESP_GATTC_READ_CHAR_EVT:
      case ESP_GATTC_READ_DESCR_EVT:
        this->event_.gattc.data = new std::vector<uint8_t>(p->read.value, p->read.value + p->read.value_len);
        this->event_.gattc.gattc_param->read.value = this->event_.gattc.data->data();
        break;
      default:
        this->event_.gattc.data = nullptr;
        break;
    }
  }

  // Constructor for GATTS events - uses heap allocation
  // Creates a copy of the param struct since the original is only valid during the callback
  BLEEvent(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
    this->type_ = GATTS;
    this->event_.gatts.gatts_event = e;
    this->event_.gatts.gatts_if = i;

    if (p == nullptr) {
      this->event_.gatts.gatts_param = nullptr;
      this->event_.gatts.data = nullptr;
      return;  // Invalid event, but we can't log in header file
    }

    // Heap-allocate param and data
    // Heap allocation is used because GATTC/GATTS events are rare (<1% of events)
    // while GAP events (99%) are stored inline to minimize memory usage
    this->event_.gatts.gatts_param = new esp_ble_gatts_cb_param_t(*p);

    // Copy data for events that need it
    switch (e) {
      case ESP_GATTS_WRITE_EVT:
        this->event_.gatts.data = new std::vector<uint8_t>(p->write.value, p->write.value + p->write.len);
        this->event_.gatts.gatts_param->write.value = this->event_.gatts.data->data();
        break;
      default:
        this->event_.gatts.data = nullptr;
        break;
    }
  }

  // Destructor to clean up heap allocations
  ~BLEEvent() {
    switch (this->type_) {
      case GATTC:
        delete this->event_.gattc.gattc_param;
        delete this->event_.gattc.data;
        break;
      case GATTS:
        delete this->event_.gatts.gatts_param;
        delete this->event_.gatts.data;
        break;
      default:
        break;
    }
  }

  // Disable copy to prevent double-delete
  BLEEvent(const BLEEvent &) = delete;
  BLEEvent &operator=(const BLEEvent &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gap_event {
      esp_gap_ble_cb_event_t gap_event;
      union {
        BLEScanResult scan_result;  // 73 bytes
        // This matches ESP-IDF's scan complete event structures
        // All three (scan_param_cmpl, scan_start_cmpl, scan_stop_cmpl) have identical layout
        struct {
          esp_bt_status_t status;
        } scan_complete;  // 1 byte
      };
    } gap;  // 80 bytes total

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gattc_event {
      esp_gattc_cb_event_t gattc_event;
      esp_gatt_if_t gattc_if;
      esp_ble_gattc_cb_param_t *gattc_param;  // Heap-allocated
      std::vector<uint8_t> *data;             // Heap-allocated
    } gattc;                                  // 16 bytes (pointers only)

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gatts_event {
      esp_gatts_cb_event_t gatts_event;
      esp_gatt_if_t gatts_if;
      esp_ble_gatts_cb_param_t *gatts_param;  // Heap-allocated
      std::vector<uint8_t> *data;             // Heap-allocated
    } gatts;                                  // 16 bytes (pointers only)
  } event_;                                   // 80 bytes

  ble_event_t type_;

  // Helper methods to access event data
  ble_event_t type() const { return type_; }
  esp_gap_ble_cb_event_t gap_event_type() const { return event_.gap.gap_event; }
  const BLEScanResult &scan_result() const { return event_.gap.scan_result; }
  esp_bt_status_t scan_complete_status() const { return event_.gap.scan_complete.status; }
};

// BLEEvent total size: 84 bytes (80 byte union + 1 byte type + 3 bytes padding)

}  // namespace esp32_ble
}  // namespace esphome

#endif
