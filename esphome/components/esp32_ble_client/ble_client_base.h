#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32_BLE_DEVICE
#include "ble_service.h"
#endif

#include <array>
#include <vector>

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>

namespace esphome::esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

static const int UNSET_CONN_ID = 0xFFFF;
static constexpr size_t MAC_ADDR_STR_LEN = 18;  // "AA:BB:CC:DD:EE:FF\0"

class BLEClientBase : public espbt::ESPBTClient, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void run_later(std::function<void()> &&f);  // NOLINT
#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const espbt::ESPBTDevice &device) override;
#endif
  void on_scan_end() override {}
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override;
  esp_err_t pair();
  void disconnect() override;
  void unconditional_disconnect();
  void release_services();

  bool connected() { return this->state_ == espbt::ClientState::ESTABLISHED; }

  void set_auto_connect(bool auto_connect) { this->auto_connect_ = auto_connect; }

  virtual void set_address(uint64_t address) {
    this->address_ = address;
    this->remote_bda_[0] = (address >> 40) & 0xFF;
    this->remote_bda_[1] = (address >> 32) & 0xFF;
    this->remote_bda_[2] = (address >> 24) & 0xFF;
    this->remote_bda_[3] = (address >> 16) & 0xFF;
    this->remote_bda_[4] = (address >> 8) & 0xFF;
    this->remote_bda_[5] = (address >> 0) & 0xFF;
    if (address == 0) {
      this->address_str_[0] = '\0';
    } else {
      format_mac_addr_upper(this->remote_bda_, this->address_str_);
    }
  }
  const char *address_str() const { return this->address_str_; }

#ifdef USE_ESP32_BLE_DEVICE
  BLEService *get_service(espbt::ESPBTUUID uuid);
  BLEService *get_service(uint16_t uuid);
  BLECharacteristic *get_characteristic(espbt::ESPBTUUID service, espbt::ESPBTUUID chr);
  BLECharacteristic *get_characteristic(uint16_t service, uint16_t chr);
  BLECharacteristic *get_characteristic(uint16_t handle);
  BLEDescriptor *get_descriptor(espbt::ESPBTUUID service, espbt::ESPBTUUID chr, espbt::ESPBTUUID descr);
  BLEDescriptor *get_descriptor(uint16_t service, uint16_t chr, uint16_t descr);
  BLEDescriptor *get_descriptor(uint16_t handle);
  // Get the configuration descriptor for the given characteristic handle.
  BLEDescriptor *get_config_descriptor(uint16_t handle);
#endif

  float parse_char_value(uint8_t *value, uint16_t length);

  int get_gattc_if() const { return this->gattc_if_; }
  uint8_t *get_remote_bda() { return this->remote_bda_; }
  esp_ble_addr_type_t get_remote_addr_type() const { return this->remote_addr_type_; }
  void set_remote_addr_type(esp_ble_addr_type_t address_type) { this->remote_addr_type_ = address_type; }
  uint16_t get_conn_id() const { return this->conn_id_; }
  uint64_t get_address() const { return this->address_; }
  bool is_paired() const { return this->paired_; }

  uint8_t get_connection_index() const { return this->connection_index_; }

  virtual void set_connection_type(espbt::ConnectionType ct) { this->connection_type_ = ct; }

  bool check_addr(esp_bd_addr_t &addr) { return memcmp(addr, this->remote_bda_, sizeof(esp_bd_addr_t)) == 0; }

  void set_state(espbt::ClientState st) override;

 protected:
  // Memory optimized layout for 32-bit systems
  // Group 1: 8-byte types
  uint64_t address_{0};

  // Group 2: Container types (grouped for memory optimization)
#ifdef USE_ESP32_BLE_DEVICE
  std::vector<BLEService *> services_;
#endif

  // Group 3: 4-byte types
  int gattc_if_;
  esp_gatt_status_t status_{ESP_GATT_OK};

  // Group 4: Arrays
  char address_str_[MAC_ADDR_STR_LEN]{};  // 18 bytes: "AA:BB:CC:DD:EE:FF\0"
  esp_bd_addr_t remote_bda_;              // 6 bytes

  // Group 5: 2-byte types
  uint16_t conn_id_{UNSET_CONN_ID};
  uint16_t mtu_{23};

  // Group 6: 1-byte types and small enums
  esp_ble_addr_type_t remote_addr_type_{BLE_ADDR_TYPE_PUBLIC};
  espbt::ConnectionType connection_type_{espbt::ConnectionType::V1};
  uint8_t connection_index_;
  uint8_t service_count_{0};  // ESP32 has max handles < 255, typical devices have < 50 services
  bool auto_connect_{false};
  bool paired_{false};
  // 6 bytes used, 2 bytes padding

  void log_event_(const char *name);
  void log_gattc_event_(const char *name);
  void update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                           const char *param_type);
  void set_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                        const char *param_type);
  void log_gattc_warning_(const char *operation, esp_gatt_status_t status);
  void log_gattc_warning_(const char *operation, esp_err_t err);
  void log_connection_params_(const char *param_type);
  void handle_connection_result_(esp_err_t ret);
  // Compact error logging helpers to reduce flash usage
  void log_error_(const char *message);
  void log_error_(const char *message, int code);
  void log_warning_(const char *message);
};

}  // namespace esphome::esp32_ble_client

#endif  // USE_ESP32
