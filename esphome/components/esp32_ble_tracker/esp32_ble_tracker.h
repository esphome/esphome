#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "queue.h"

#ifdef USE_ESP32

#include <string>
#include <array>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace esp32_ble_tracker {

class ESPBTUUID {
 public:
  ESPBTUUID();

  static ESPBTUUID from_uint16(uint16_t uuid);

  static ESPBTUUID from_uint32(uint32_t uuid);

  static ESPBTUUID from_raw(const uint8_t *data);

  static ESPBTUUID from_raw(const std::string &data);

  static ESPBTUUID from_uuid(esp_bt_uuid_t uuid);

  ESPBTUUID as_128bit() const;

  bool contains(uint8_t data1, uint8_t data2) const;

  bool operator==(const ESPBTUUID &uuid) const;
  bool operator!=(const ESPBTUUID &uuid) const { return !(*this == uuid); }

  esp_bt_uuid_t get_uuid() const;

  std::string to_string() const;

 protected:
  esp_bt_uuid_t uuid_;
};

using adv_data_t = std::vector<uint8_t>;

struct ServiceData {
  ESPBTUUID uuid;
  adv_data_t data;
};

class ESPBLEiBeacon {
 public:
  ESPBLEiBeacon() { memset(&this->beacon_data_, 0, sizeof(this->beacon_data_)); }
  ESPBLEiBeacon(const uint8_t *data);
  static optional<ESPBLEiBeacon> from_manufacturer_data(const ServiceData &data);

  uint16_t get_major() { return ((this->beacon_data_.major & 0xFF) << 8) | (this->beacon_data_.major >> 8); }
  uint16_t get_minor() { return ((this->beacon_data_.minor & 0xFF) << 8) | (this->beacon_data_.minor >> 8); }
  int8_t get_signal_power() { return this->beacon_data_.signal_power; }
  ESPBTUUID get_uuid() { return ESPBTUUID::from_raw(this->beacon_data_.proximity_uuid); }

 protected:
  struct {
    uint8_t sub_type;
    uint8_t length;
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

  const uint8_t *address() const { return address_; }

  esp_ble_addr_type_t get_address_type() const { return this->address_type_; }
  int get_rssi() const { return rssi_; }
  const std::string &get_name() const { return this->name_; }

  const std::vector<int8_t> &get_tx_powers() const { return tx_powers_; }

  const optional<uint16_t> &get_appearance() const { return appearance_; }
  const optional<uint8_t> &get_ad_flag() const { return ad_flag_; }
  const std::vector<ESPBTUUID> &get_service_uuids() const { return service_uuids_; }

  const std::vector<ServiceData> &get_manufacturer_datas() const { return manufacturer_datas_; }

  const std::vector<ServiceData> &get_service_datas() const { return service_datas_; }

  optional<ESPBLEiBeacon> get_ibeacon() const {
    for (auto &it : this->manufacturer_datas_) {
      auto res = ESPBLEiBeacon::from_manufacturer_data(it);
      if (res.has_value())
        return *res;
    }
    return {};
  }

 protected:
  void parse_adv_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);

  esp_bd_addr_t address_{
      0,
  };
  esp_ble_addr_type_t address_type_{BLE_ADDR_TYPE_PUBLIC};
  int rssi_{0};
  std::string name_{};
  std::vector<int8_t> tx_powers_{};
  optional<uint16_t> appearance_{};
  optional<uint8_t> ad_flag_{};
  std::vector<ESPBTUUID> service_uuids_;
  std::vector<ServiceData> manufacturer_datas_{};
  std::vector<ServiceData> service_datas_{};
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

enum class ClientState {
  // Connection is idle, no device detected.
  IDLE,
  // Device advertisement found.
  DISCOVERED,
  // Connection in progress.
  CONNECTING,
  // Initial connection established.
  CONNECTED,
  // The client and sub-clients have completed setup.
  ESTABLISHED,
};

class ESPBTClient : public ESPBTDeviceListener {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
  virtual void connect() = 0;
  void set_state(ClientState st) { this->state_ = st; }
  ClientState state() const { return state_; }
  int app_id;

 protected:
  ClientState state_;
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
  float get_setup_priority() const override;

  void loop() override;

  void register_listener(ESPBTDeviceListener *listener) {
    listener->set_parent(this);
    this->listeners_.push_back(listener);
  }

  void register_client(ESPBTClient *client);

  void print_bt_device_info(const ESPBTDevice &device);

 protected:
  /// The FreeRTOS task managing the bluetooth interface.
  static bool ble_setup();
  /// Start a single scan by setting up the parameters and doing some esp-idf calls.
  void start_scan_(bool first);
  /// Callback that will handle all GAP events and redistribute them to other callbacks.
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  /// Called when a `ESP_GAP_BLE_SCAN_RESULT_EVT` event is received.
  void gap_scan_result_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` event is received.
  void gap_scan_set_param_complete_(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_START_COMPLETE_EVT` event is received.
  void gap_scan_start_complete_(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT` event is received.
  void gap_scan_stop_complete_(const esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param &param);

  int app_id_;
  /// Callback that will handle all GATTC events and redistribute them to other callbacks.
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

  /// Vector of addresses that have already been printed in print_bt_device_info
  std::vector<uint64_t> already_discovered_;
  std::vector<ESPBTDeviceListener *> listeners_;
  /// Client parameters.
  std::vector<ESPBTClient *> clients_;
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

  Queue<BLEEvent> ble_events_;
};

// NOLINTNEXTLINE
extern ESP32BLETracker *global_esp32_ble_tracker;

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
