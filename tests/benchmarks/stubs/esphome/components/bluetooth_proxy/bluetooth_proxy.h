// Stub for benchmark builds — provides the minimal interface that
// api_connection.cpp needs when USE_BLUETOOTH_PROXY is defined,
// without pulling in ESP32 BLE dependencies.
#pragma once

#include "esphome/components/api/api_pb2.h"

namespace esphome {
namespace api {
class APIConnection;
}  // namespace api

namespace bluetooth_proxy {

class BluetoothProxy {
 public:
  api::APIConnection *get_api_connection() const { return nullptr; }
  void subscribe_api_connection(api::APIConnection *conn, uint32_t flags) {}
  void unsubscribe_api_connection(api::APIConnection *conn) {}
  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {}
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {}
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {}
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {}
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {}
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {}
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {}
  void send_connections_free(api::APIConnection *conn) {}
  void bluetooth_scanner_set_mode(bool active) {}
  void bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg) {}
  uint32_t get_feature_flags() const { return 0; }
  void get_bluetooth_mac_address_pretty(char *buf) const { buf[0] = '\0'; }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern BluetoothProxy *global_bluetooth_proxy;

}  // namespace bluetooth_proxy
}  // namespace esphome
