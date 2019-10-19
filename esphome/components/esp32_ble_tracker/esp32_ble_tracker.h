#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef ARDUINO_ARCH_ESP32

#include <string>
#include <array>
#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace esp32_ble_tracker {

class ESPBTUUID {
 public:
  ESPBTUUID();

  static ESPBTUUID from_uint16(uint16_t uuid);

  static ESPBTUUID from_uint32(uint32_t uuid);

  static ESPBTUUID from_raw(const uint8_t *data);

  bool contains(uint8_t data1, uint8_t data2) const;

  std::string to_string();

 protected:
  esp_bt_uuid_t uuid_;
};

class ESPBLEiBeacon {
 public:
  ESPBLEiBeacon() { memset(&this->beacon_data_, 0, sizeof(this->beacon_data_)); }
  ESPBLEiBeacon(const uint8_t *data);
  static optional<ESPBLEiBeacon> from_manufacturer_data(const std::string &data);

  uint16_t get_major() { return reverse_bits_16(this->beacon_data_.major); }
  uint16_t get_minor() { return reverse_bits_16(this->beacon_data_.minor); }
  int8_t get_signal_power() { return this->beacon_data_.signal_power; }
  ESPBTUUID get_uuid() { return ESPBTUUID::from_raw(this->beacon_data_.proximity_uuid); }

 protected:
  struct {
    uint16_t manufacturer_id;
    uint8_t sub_type;
    uint8_t proximity_uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t signal_power;
  } PACKED beacon_data_;
};

class ESPBTDevice {
 public:
  void parse_scan_rst(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);

  std::string address_str() const;

  uint64_t address_uint64() const;

  esp_ble_addr_type_t get_address_type() const;
  int get_rssi() const;
  const std::string &get_name() const;
  const optional<int8_t> &get_tx_power() const;
  const optional<uint16_t> &get_appearance() const;
  const optional<uint8_t> &get_ad_flag() const;
  const std::vector<ESPBTUUID> &get_service_uuids() const;
  const std::string &get_manufacturer_data() const;
  const std::string &get_service_data() const;
  const optional<ESPBTUUID> &get_service_data_uuid() const;
  const optional<ESPBLEiBeacon> get_ibeacon() const {
    return ESPBLEiBeacon::from_manufacturer_data(this->manufacturer_data_);
  }

 protected:
  void parse_adv_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);

  esp_bd_addr_t address_{
      0,
  };
  esp_ble_addr_type_t address_type_{BLE_ADDR_TYPE_PUBLIC};
  int rssi_{0};
  std::string name_{};
  optional<int8_t> tx_power_{};
  optional<uint16_t> appearance_{};
  optional<uint8_t> ad_flag_{};
  std::vector<ESPBTUUID> service_uuids_;
  std::string manufacturer_data_{};
  std::string service_data_{};
  optional<ESPBTUUID> service_data_uuid_{};
};

class ESP32BLETracker;

class ESPBTDeviceListener {
 public:
  virtual void on_scan_end() {}
  virtual bool parse_device(const ESPBTDevice &device) = 0;
  void set_parent(ESP32BLETracker *parent) { parent_ = parent; }

 protected:
  ESP32BLETracker *parent_{nullptr};
};

class ESP32BLETracker : public Component {
 public:
  void set_scan_duration(uint32_t scan_duration) { scan_duration_ = scan_duration; }
  void set_scan_interval(uint32_t scan_interval) { scan_interval_ = scan_interval; }
  void set_scan_window(uint32_t scan_window) { scan_window_ = scan_window; }
  void set_scan_active(bool scan_active) { scan_active_ = scan_active; }

  /// Setup the FreeRTOS task and the Bluetooth stack.
  void setup() override;
  void dump_config() override;

  void loop() override;

  void register_listener(ESPBTDeviceListener *listener) {
    listener->set_parent(this);
    this->listeners_.push_back(listener);
  }

  void print_bt_device_info(const ESPBTDevice &device);

 protected:
  /// The FreeRTOS task managing the bluetooth interface.
  static bool ble_setup();
  /// Start a single scan by setting up the parameters and doing some esp-idf calls.
  void start_scan(bool first);
  /// Callback that will handle all GAP events and redistribute them to other callbacks.
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  /// Called when a `ESP_GAP_BLE_SCAN_RESULT_EVT` event is received.
  void gap_scan_result(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` event is received.
  void gap_scan_set_param_complete(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_START_COMPLETE_EVT` event is received.
  void gap_scan_start_complete(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param);

  /// Vector of addresses that have already been printed in print_bt_device_info
  std::vector<uint64_t> already_discovered_;
  std::vector<ESPBTDeviceListener *> listeners_;
  /// A structure holding the ESP BLE scan parameters.
  esp_ble_scan_params_t scan_params_;
  /// The interval in seconds to perform scans.
  uint32_t scan_duration_;
  uint32_t scan_interval_;
  uint32_t scan_window_;
  bool scan_active_;
  SemaphoreHandle_t scan_result_lock_;
  SemaphoreHandle_t scan_end_lock_;
  size_t scan_result_index_{0};
  esp_ble_gap_cb_param_t::ble_scan_result_evt_param scan_result_buffer_[16];
  esp_bt_status_t scan_start_failed_{ESP_BT_STATUS_SUCCESS};
  esp_bt_status_t scan_set_param_failed_{ESP_BT_STATUS_SUCCESS};
};

extern ESP32BLETracker *global_esp32_ble_tracker;

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
