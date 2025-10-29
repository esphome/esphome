#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_client/ble_client_base.h"

namespace esphome::bluetooth_proxy {

class BluetoothProxy;

class BluetoothConnection final : public esp32_ble_client::BLEClientBase {
 public:
  void dump_config() override;
  void loop() override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  esp32_ble_tracker::AdvertisementParserType get_advertisement_parser_type() override;

  esp_err_t read_characteristic(uint16_t handle);
  esp_err_t write_characteristic(uint16_t handle, const uint8_t *data, size_t length, bool response);
  esp_err_t read_descriptor(uint16_t handle);
  esp_err_t write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response);

  esp_err_t notify_characteristic(uint16_t handle, bool enable);

  void set_address(uint64_t address) override;

 protected:
  friend class BluetoothProxy;

  bool supports_efficient_uuids_() const;
  void send_service_for_discovery_();
  void reset_connection_(esp_err_t reason);
  void update_allocated_slot_(uint64_t find_value, uint64_t set_value);
  void log_connection_error_(const char *operation, esp_gatt_status_t status);
  void log_connection_warning_(const char *operation, esp_err_t err);
  void log_gatt_not_connected_(const char *action, const char *type);
  void log_gatt_operation_error_(const char *operation, uint16_t handle, esp_gatt_status_t status);
  esp_err_t check_and_log_error_(const char *operation, esp_err_t err);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  BluetoothProxy *proxy_;

  // Group 2: 2-byte types
  int16_t send_service_{-3};  // -3 = INIT_SENDING_SERVICES, -2 = DONE_SENDING_SERVICES, >=0 = service index

  // Group 3: 1-byte types
  bool seen_mtu_or_services_{false};
  // 1 byte used, 1 byte padding
};

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32
