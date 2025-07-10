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
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_param_cmpl.status) == 0,
              "status must be first member of scan_param_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_start_cmpl.status) == 0,
              "status must be first member of scan_start_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_stop_cmpl.status) == 0,
              "status must be first member of scan_stop_cmpl");

// Compile-time verification for advertising complete events
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_adv_data_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF adv_data_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_scan_rsp_data_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF scan_rsp_data_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_adv_data_raw_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF adv_data_raw_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_adv_start_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF adv_start_cmpl structure has unexpected size");
static_assert(sizeof(esp_ble_gap_cb_param_t::ble_adv_stop_cmpl_evt_param) == sizeof(esp_bt_status_t),
              "ESP-IDF adv_stop_cmpl structure has unexpected size");

// Verify the status field is at offset 0 for advertising events
static_assert(offsetof(esp_ble_gap_cb_param_t, adv_data_cmpl.status) == 0,
              "status must be first member of adv_data_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, scan_rsp_data_cmpl.status) == 0,
              "status must be first member of scan_rsp_data_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, adv_data_raw_cmpl.status) == 0,
              "status must be first member of adv_data_raw_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, adv_start_cmpl.status) == 0,
              "status must be first member of adv_start_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, adv_stop_cmpl.status) == 0,
              "status must be first member of adv_stop_cmpl");

// Compile-time verification for RSSI complete event structure
static_assert(offsetof(esp_ble_gap_cb_param_t, read_rssi_cmpl.status) == 0,
              "status must be first member of read_rssi_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, read_rssi_cmpl.rssi) == sizeof(esp_bt_status_t),
              "rssi must immediately follow status in read_rssi_cmpl");
static_assert(offsetof(esp_ble_gap_cb_param_t, read_rssi_cmpl.remote_addr) == sizeof(esp_bt_status_t) + sizeof(int8_t),
              "remote_addr must follow rssi in read_rssi_cmpl");

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
//
// CRITICAL DESIGN NOTE:
// The heap allocations for GATTC/GATTS events are REQUIRED for memory safety.
// DO NOT attempt to optimize by removing these allocations or storing pointers
// to the original ESP-IDF data. The ESP-IDF callback data has a different lifetime
// than our event processing, and accessing it after the callback returns would
// result in use-after-free bugs and crashes.
class BLEEvent {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum ble_event_t : uint8_t {
    GAP,
    GATTC,
    GATTS,
  };

  // Type definitions for cleaner method signatures
  struct StatusOnlyData {
    esp_bt_status_t status;
  };

  struct RSSICompleteData {
    esp_bt_status_t status;
    int8_t rssi;
    esp_bd_addr_t remote_addr;
  };

  // Constructor for GAP events - no external allocations needed
  BLEEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->type_ = GAP;
    this->init_gap_data_(e, p);
  }

  // Constructor for GATTC events - uses heap allocation
  // IMPORTANT: The heap allocation is REQUIRED and must not be removed as an optimization.
  // The param pointer from ESP-IDF is only valid during the callback execution.
  // Since BLE events are processed asynchronously in the main loop, we must create
  // our own copy to ensure the data remains valid until the event is processed.
  BLEEvent(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    this->type_ = GATTC;
    this->init_gattc_data_(e, i, p);
  }

  // Constructor for GATTS events - uses heap allocation
  // IMPORTANT: The heap allocation is REQUIRED and must not be removed as an optimization.
  // The param pointer from ESP-IDF is only valid during the callback execution.
  // Since BLE events are processed asynchronously in the main loop, we must create
  // our own copy to ensure the data remains valid until the event is processed.
  BLEEvent(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
    this->type_ = GATTS;
    this->init_gatts_data_(e, i, p);
  }

  // Destructor to clean up heap allocations
  ~BLEEvent() { this->release(); }

  // Default constructor for pre-allocation in pool
  BLEEvent() : type_(GAP) {}

  // Invoked on return to EventPool - clean up any heap-allocated data
  void release() {
    if (this->type_ == GAP) {
      return;
    }
    if (this->type_ == GATTC) {
      delete this->event_.gattc.gattc_param;
      delete this->event_.gattc.data;
      this->event_.gattc.gattc_param = nullptr;
      this->event_.gattc.data = nullptr;
      return;
    }
    if (this->type_ == GATTS) {
      delete this->event_.gatts.gatts_param;
      delete this->event_.gatts.data;
      this->event_.gatts.gatts_param = nullptr;
      this->event_.gatts.data = nullptr;
    }
  }

  // Load new event data for reuse (replaces previous event data)
  void load_gap_event(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->release();
    this->type_ = GAP;
    this->init_gap_data_(e, p);
  }

  void load_gattc_event(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    this->release();
    this->type_ = GATTC;
    this->init_gattc_data_(e, i, p);
  }

  void load_gatts_event(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
    this->release();
    this->type_ = GATTS;
    this->init_gatts_data_(e, i, p);
  }

  // Disable copy to prevent double-delete
  BLEEvent(const BLEEvent &) = delete;
  BLEEvent &operator=(const BLEEvent &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gap_event {
      esp_gap_ble_cb_event_t gap_event;
      union {
        BLEScanResult scan_result;  // 73 bytes - Used by: esp32_ble_tracker
        // This matches ESP-IDF's scan complete event structures
        // All three (scan_param_cmpl, scan_start_cmpl, scan_stop_cmpl) have identical layout
        // Used by: esp32_ble_tracker
        StatusOnlyData scan_complete;  // 1 byte
        // Advertising complete events all have same structure
        // Used by: esp32_ble_beacon, esp32_ble server components
        // ADV_DATA_SET, SCAN_RSP_DATA_SET, ADV_DATA_RAW_SET, ADV_START, ADV_STOP
        StatusOnlyData adv_complete;  // 1 byte
        // RSSI complete event
        // Used by: ble_client (ble_rssi_sensor component)
        RSSICompleteData read_rssi_complete;  // 8 bytes
        // Security events - we store the full security union
        // Used by: ble_client (automation), bluetooth_proxy, esp32_ble_client
        esp_ble_sec_t security;  // Variable size, but fits within scan_result size
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
  esp_bt_status_t adv_complete_status() const { return event_.gap.adv_complete.status; }
  const RSSICompleteData &read_rssi_complete() const { return event_.gap.read_rssi_complete; }
  const esp_ble_sec_t &security() const { return event_.gap.security; }

 private:
  // Initialize GAP event data
  void init_gap_data_(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->event_.gap.gap_event = e;

    if (p == nullptr) {
      return;  // Invalid event, but we can't log in header file
    }

    // Copy data based on event type
    switch (e) {
      case ESP_GAP_BLE_SCAN_RESULT_EVT:
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

      // Advertising complete events - all have same structure with just status
      // Used by: esp32_ble_beacon, esp32_ble server components
      case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        this->event_.gap.adv_complete.status = p->adv_data_cmpl.status;
        break;
      case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        this->event_.gap.adv_complete.status = p->scan_rsp_data_cmpl.status;
        break;
      case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:  // Used by: esp32_ble_beacon
        this->event_.gap.adv_complete.status = p->adv_data_raw_cmpl.status;
        break;
      case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:  // Used by: esp32_ble_beacon
        this->event_.gap.adv_complete.status = p->adv_start_cmpl.status;
        break;
      case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:  // Used by: esp32_ble_beacon
        this->event_.gap.adv_complete.status = p->adv_stop_cmpl.status;
        break;

      // RSSI complete event
      // Used by: ble_client (ble_rssi_sensor)
      case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
        this->event_.gap.read_rssi_complete.status = p->read_rssi_cmpl.status;
        this->event_.gap.read_rssi_complete.rssi = p->read_rssi_cmpl.rssi;
        memcpy(this->event_.gap.read_rssi_complete.remote_addr, p->read_rssi_cmpl.remote_addr, sizeof(esp_bd_addr_t));
        break;

      // Security events - copy the entire security union
      // Used by: ble_client, bluetooth_proxy, esp32_ble_client
      case ESP_GAP_BLE_AUTH_CMPL_EVT:      // Used by: bluetooth_proxy, esp32_ble_client
      case ESP_GAP_BLE_SEC_REQ_EVT:        // Used by: esp32_ble_client
      case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  // Used by: ble_client automation
      case ESP_GAP_BLE_PASSKEY_REQ_EVT:    // Used by: ble_client automation
      case ESP_GAP_BLE_NC_REQ_EVT:         // Used by: ble_client automation
        memcpy(&this->event_.gap.security, &p->ble_security, sizeof(esp_ble_sec_t));
        break;

      default:
        // We only store data for GAP events that components currently use
        // Unknown events still get queued and logged in ble.cpp:375 as
        // "Unhandled GAP event type in loop" - this helps identify new events
        // that components might need in the future
        break;
    }
  }

  // Initialize GATTC event data
  void init_gattc_data_(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
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
    // IMPORTANT: This heap allocation provides clear ownership semantics:
    // - The BLEEvent owns the allocated memory for its lifetime
    // - The data remains valid from the BLE callback context until processed in the main loop
    // - Without this copy, we'd have use-after-free bugs as ESP-IDF reuses the callback memory
    this->event_.gattc.gattc_param = new esp_ble_gattc_cb_param_t(*p);

    // Copy data for events that need it
    // The param struct contains pointers (e.g., notify.value) that point to temporary buffers.
    // We must copy this data to ensure it remains valid when the event is processed later.
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

  // Initialize GATTS event data
  void init_gatts_data_(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
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
    // IMPORTANT: This heap allocation provides clear ownership semantics:
    // - The BLEEvent owns the allocated memory for its lifetime
    // - The data remains valid from the BLE callback context until processed in the main loop
    // - Without this copy, we'd have use-after-free bugs as ESP-IDF reuses the callback memory
    this->event_.gatts.gatts_param = new esp_ble_gatts_cb_param_t(*p);

    // Copy data for events that need it
    // The param struct contains pointers (e.g., write.value) that point to temporary buffers.
    // We must copy this data to ensure it remains valid when the event is processed later.
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
};

// Verify the gap_event struct hasn't grown beyond expected size
// The gap member in the union should be 80 bytes (including the gap_event enum)
static_assert(sizeof(decltype(((BLEEvent *) nullptr)->event_.gap)) <= 80, "gap_event struct has grown beyond 80 bytes");

// Verify esp_ble_sec_t fits within our union
static_assert(sizeof(esp_ble_sec_t) <= 73, "esp_ble_sec_t is larger than BLEScanResult");

// BLEEvent total size: 84 bytes (80 byte union + 1 byte type + 3 bytes padding)

}  // namespace esp32_ble
}  // namespace esphome

#endif
