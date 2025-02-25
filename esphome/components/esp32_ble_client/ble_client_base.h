#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include "ble_service.h"

#include <array>
#include <string>
#include <vector>

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

static const int UNSET_CONN_ID = 0xFFFF;

class BLEClientBase : public espbt::ESPBTClient, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void run_later(std::function<void()> &&f);  // NOLINT
  bool parse_device(const espbt::ESPBTDevice &device) override;
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

  void set_address(uint64_t address) {
    this->address_ = address;
    this->remote_bda_[0] = (address >> 40) & 0xFF;
    this->remote_bda_[1] = (address >> 32) & 0xFF;
    this->remote_bda_[2] = (address >> 24) & 0xFF;
    this->remote_bda_[3] = (address >> 16) & 0xFF;
    this->remote_bda_[4] = (address >> 8) & 0xFF;
    this->remote_bda_[5] = (address >> 0) & 0xFF;
    if (address == 0) {
      this->address_str_ = "";
    } else {
      this->address_str_ =
          str_snprintf("%02X:%02X:%02X:%02X:%02X:%02X", 17, (uint8_t) (this->address_ >> 40) & 0xff,
                       (uint8_t) (this->address_ >> 32) & 0xff, (uint8_t) (this->address_ >> 24) & 0xff,
                       (uint8_t) (this->address_ >> 16) & 0xff, (uint8_t) (this->address_ >> 8) & 0xff,
                       (uint8_t) (this->address_ >> 0) & 0xff);
    }
  }
  std::string address_str() const { return this->address_str_; }

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

 protected:
  int gattc_if_;
  esp_bd_addr_t remote_bda_;
  esp_ble_addr_type_t remote_addr_type_{BLE_ADDR_TYPE_PUBLIC};
  uint16_t conn_id_{UNSET_CONN_ID};
  uint64_t address_{0};
  bool auto_connect_{false};
  std::string address_str_{};
  uint8_t connection_index_;
  int16_t service_count_{0};
  uint16_t mtu_{23};
  bool paired_{false};
  espbt::ConnectionType connection_type_{espbt::ConnectionType::V1};
  std::vector<BLEService *> services_;
  esp_gatt_status_t status_{ESP_GATT_OK};

  void log_event_(const char *name);
};

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
