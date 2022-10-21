#pragma once

#ifdef USE_ESP32

#include <map>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <map>

namespace esphome {
namespace bluetooth_proxy {

using namespace esp32_ble_client;

class BluetoothProxy : public BLEClientBase {
 public:
  BluetoothProxy();
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);

  int get_bluetooth_connections_free() { return this->state_ == espbt::ClientState::IDLE ? 1 : 0; }
  int get_bluetooth_connections_limit() { return 1; }

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);

  int16_t send_service_{-1};
  bool active_;
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
