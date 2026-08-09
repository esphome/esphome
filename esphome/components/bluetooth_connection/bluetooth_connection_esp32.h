#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_client/ble_client_base.h"

#include "bluetooth_connection.h"

namespace esphome::bluetooth_proxy {
class BluetoothProxy;
}  // namespace esphome::bluetooth_proxy

namespace esphome::bluetooth_connection {

class BluetoothConnection final : public esp32_ble_client::BLEClientBase {
 public:
  void dump_config() override;
  void loop() override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  // The proxy's connections never consume parsed ESPBTDevice objects.
  bool wants_parsed_advertisements() override { return false; }

  esp_err_t read_characteristic(uint16_t handle);
  esp_err_t write_characteristic(uint16_t handle, const uint8_t *data, size_t length, bool response);
  esp_err_t read_descriptor(uint16_t handle);
  esp_err_t write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response);

  esp_err_t notify_characteristic(uint16_t handle, bool enable);

  esp_err_t update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout) {
    return this->update_conn_params_(min_interval, max_interval, latency, timeout, "custom");
  }

  bool has_gatt_services() const { return this->service_count_ != 0; }

  /// Start connecting: record the API address type and hand the client to the
  /// tracker's promote loop (it pauses the scan and opens the connection).
  void initiate_connection(uint8_t address_type) {
    this->set_remote_addr_type(static_cast<esp_ble_addr_type_t>(address_type));
    this->set_state(esp32_ble_tracker::ClientState::DISCOVERED);
  }

  void set_address(uint64_t address) override;

 protected:
  friend class bluetooth_proxy::BluetoothProxy;

  void on_disconnect_complete(esp_err_t reason) override;

  void send_service_for_discovery_();
  void reset_connection_(esp_err_t reason);
  void log_connection_error_(const char *operation, esp_gatt_status_t status);
  void log_connection_warning_(const char *operation, esp_err_t err);
  void log_gatt_not_connected_(const char *action, const char *type);
  void log_gatt_operation_error_(const char *operation, uint16_t handle, esp_gatt_status_t status);
  esp_err_t check_and_log_error_(const char *operation, esp_err_t err);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  bluetooth_proxy::BluetoothProxy *proxy_;

  // Group 2: 2-byte types
  int16_t send_service_{INIT_SENDING_SERVICES};  // see bluetooth_connection.h cursor states

  // Group 3: 1-byte types
  bool seen_mtu_or_services_{false};
  // 1 byte used, 1 byte padding
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32
