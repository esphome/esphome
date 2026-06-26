#pragma once

// bluetooth_proxy_libretiny.h
//
// The LibreTiny (non-ESP32) face of the hub-agnostic bluetooth_proxy. Same component,
// same class name (esphome::bluetooth_proxy::BluetoothProxy) and same API surface as the
// ESP32 proxy, so api_connection.cpp talks to one type on every platform. It inherits the
// shared BluetoothProxyAdvertisements core (identical batching/flush as ESP32) and is fed
// raw advertisements by a tracker hub (BleProxyHub). GATT active connections are ESP32-only,
// so those API entry points are no-ops here.
//
// Threading: the tracker hub owns the BLE-task -> main-loop handoff and delivers each
// advertisement to on_advertisement() on the main loop. on_advertisement() feeds the shared
// add_advertisement() batching core directly, so no synchronization is needed here — exactly
// like ESP32, where esp32_ble_tracker also delivers on the main loop (parse_devices()).

#ifdef USE_LIBRETINY

#include "bluetooth_proxy_advertisements.h"
#include "bluetooth_proxy_hub.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::api {
class APIConnection;
}  // namespace esphome::api

namespace esphome::bluetooth_proxy {

class BluetoothProxy : public BluetoothProxyAdvertisements, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // The tracker hub that supplies advertisements, adapter MAC and scan state.
  void set_hub(BleProxyHub *hub) { this->hub_ = hub; }

  // ---- bluetooth_proxy::BluetoothProxy API surface (called by api_connection.cpp) ----
  uint32_t get_feature_flags() const {
    // Advertisement proxy only: active GATT connections are ESP32-only.
    return FEATURE_PASSIVE_SCAN | FEATURE_RAW_ADVERTISEMENTS | FEATURE_STATE_AND_MODE;
  }
  void get_bluetooth_mac_address_pretty(char output[18]);
  void subscribe_api_connection(api::APIConnection *conn, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *conn);

  // GATT / active-connection entry points — no-ops off-ESP32 (never reached while
  // FEATURE_ACTIVE_CONNECTIONS is absent), present only so api_connection.cpp links.
  void bluetooth_device_request(const api::BluetoothDeviceRequest &) {}
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &) {}
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &) {}
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &) {}
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &) {}
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &) {}
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &) {}
  void bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &) {}
  void bluetooth_scanner_set_mode(bool /*active*/) {}
  void send_connections_free() {}
  void send_connections_free(api::APIConnection *) {}

  // Called by the hub on the main loop for each raw advertisement (the hub owns the
  // BLE-task -> main-loop handoff, so this runs single-threaded like the ESP32 proxy).
  void on_advertisement(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len);

 protected:
  BleProxyHub *hub_{nullptr};
};

// Looked up by api_connection.cpp to route BLE subscription/GATT messages.
extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_LIBRETINY
