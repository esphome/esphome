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

  static void uint64_to_bd_addr(uint64_t address, esp_bd_addr_t bd_addr) {
    bd_addr[0] = (address >> 40) & 0xff;
    bd_addr[1] = (address >> 32) & 0xff;
    bd_addr[2] = (address >> 24) & 0xff;
    bd_addr[3] = (address >> 16) & 0xff;
    bd_addr[4] = (address >> 8) & 0xff;
    bd_addr[5] = (address >> 0) & 0xff;
  }

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);

  BluetoothConnection *get_connection_(uint64_t address, bool reserve);

  bool active_;

  std::vector<BluetoothConnection *> connections_{};
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Version 1: Initial version without active connections
// Version 2: Support for active connections
// Version 3: New connection API
// Version 4: Pairing support
// Version 5: Cache clear support
static const uint32_t ACTIVE_CONNECTIONS_VERSION = 5;
static const uint32_t PASSIVE_ONLY_VERSION = 1;

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
