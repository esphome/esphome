#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include "ble_service.h"

#include <array>
#include <string>

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEClientBase : public espbt::ESPBTClient, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;

  bool parse_device(const espbt::ESPBTDevice &device) override;
  void on_scan_end() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override;

  void set_address(uint64_t address) { this->address_ = address; }
  std::string address_str() const;

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
  uint16_t get_conn_id() const { return this->conn_id_; }
  uint64_t get_address() const { return this->address_; }

 protected:
  int gattc_if_;
  esp_bd_addr_t remote_bda_;
  esp_ble_addr_type_t remote_addr_type_;
  uint16_t conn_id_;
  uint64_t address_;
  uint16_t mtu_{23};

  std::vector<BLEService *> services_;
};

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
