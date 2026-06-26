#pragma once

// Neutral advertisement-forwarding core shared by the ESP32 and LibreTiny Bluetooth
// proxies. This is the platform-independent half of the original ESP32 bluetooth_proxy:
// it batches raw BLE advertisements into a BluetoothLERawAdvertisementsResponse and
// flushes them to the subscribed Home Assistant API connection. It contains NO
// chip-specific types — only the scan source (which feeds add_advertisement) and GATT
// connections are platform-specific and live in the per-platform proxy subclass.
//
// Threading: add_advertisement() runs on the main loop. On ESP32, esp32_ble_tracker
// already delivers advertisements there; the LibreTiny proxy drains its BLE-task queue
// into add_advertisement() from its own loop(), so the batching is single-threaded on
// both platforms — identical to the original ESP32 behaviour.

#include "esphome/core/defines.h"

// Shared by the ESP32 and LibreTiny proxies only; empty on every other platform.
#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"

#include <cstdint>

namespace esphome::bluetooth_proxy {

// BLE feature flags reported to Home Assistant via DeviceInfoResponse.
enum BluetoothProxyFeature : uint32_t {
  FEATURE_PASSIVE_SCAN = 1 << 0,
  FEATURE_ACTIVE_CONNECTIONS = 1 << 1,
  FEATURE_REMOTE_CACHING = 1 << 2,
  FEATURE_PAIRING = 1 << 3,
  FEATURE_CACHE_CLEARING = 1 << 4,
  FEATURE_RAW_ADVERTISEMENTS = 1 << 5,
  FEATURE_STATE_AND_MODE = 1 << 6,
  FEATURE_CONNECTION_PARAMS_SETTING = 1 << 7,
};

enum BluetoothProxySubscriptionFlag : uint32_t {
  SUBSCRIPTION_RAW_ADVERTISEMENTS = 1 << 0,
};

// Advertisement batching core (was inline in the ESP32 BluetoothProxy). Owns the
// subscribed API connection and the in-flight response buffer; the per-platform proxy
// subclasses inherit this and feed it advertisements.
class BluetoothProxyAdvertisements {
 public:
  // Append one raw advertisement to the current batch, flushing automatically once the
  // batch reaches BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE. No-op if not subscribed.
  // Must be called on the main loop.
  void add_advertisement(uint64_t address, int rssi, uint8_t address_type, const uint8_t *data, uint16_t data_len);

  api::APIConnection *get_api_connection() const { return this->api_connection_; }

 protected:
  // Send the current batch to the API connection and reset it. Caller ensures the API
  // server is connected and api_connection_ is non-null.
  void flush_pending_advertisements_();
  void log_advertisement_flush_();

  api::APIConnection *api_connection_{nullptr};
  api::BluetoothLERawAdvertisementsResponse response_;
  uint32_t last_advertisement_flush_time_{0};
};

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32 || USE_LIBRETINY
