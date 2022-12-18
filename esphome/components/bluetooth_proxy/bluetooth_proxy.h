#pragma once

#ifdef USE_ESP32

#include <map>
#include <vector>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include "bluetooth_connection.h"

namespace esphome {
namespace bluetooth_proxy {

static const esp_err_t ESP_GATT_NOT_CONNECTED = -1;

using namespace esp32_ble_client;

class BluetoothProxy : public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  BluetoothProxy();
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  void loop() override;

  void register_connection(BluetoothConnection *connection) {
    this->connections_.push_back(connection);
    connection->proxy_ = this;
  }

  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);

  int get_bluetooth_connections_free();
  int get_bluetooth_connections_limit() { return this->connections_.size(); }

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);

  BluetoothConnection *get_connection_(uint64_t address, bool reserve);

  bool active_;

  std::vector<BluetoothConnection *> connections_{};
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
