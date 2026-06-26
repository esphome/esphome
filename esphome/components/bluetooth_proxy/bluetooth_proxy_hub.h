#pragma once

// bluetooth_proxy_hub.h
//
// Neutral hub interface that a LibreTiny BLE tracker (bk72xx_ble_tracker,
// ln882h_ble_tracker, …) implements so the generic, chip-agnostic BluetoothProxy can
// forward its advertisements without knowing the chip. This header lives in the
// bluetooth_proxy component, so it is present only when a proxy is configured; trackers
// include it and inherit BleProxyHub under #ifdef USE_BLUETOOTH_PROXY, and otherwise build
// stand-alone (BLE sensors, no proxy).
//
// On ESP32 the proxy gets its advertisements from esp32_ble_tracker directly, so this
// interface is only used off-ESP32.

#ifdef USE_LIBRETINY

#include <cstdint>
#include <functional>

namespace esphome::bluetooth_proxy {

// Raw BLE advertisement callback. The hub invokes it on the main loop (it owns the
// BLE-task -> main-loop handoff), so the proxy can forward without any synchronization.
//
// BYTE-ORDER CONTRACT (authoritative — hub implementers must follow this):
//   mac[] is delivered LEAST-significant octet first (mac[0] = LSB). The proxy assembles
//   the 64-bit device address as sum(mac[i] << (i*8)), which is identical to ESP32's
//   esp32_ble::ble_addr_to_uint64(), so Home Assistant decodes the device MAC correctly.
//   BLE/HCI controllers report addresses LSB-first, so a hub normally passes the
//   controller's address bytes through unchanged. (This is the opposite order from
//   get_proxy_mac(), which returns the adapter MAC most-significant octet first.)
using ScanResultCallback =
    std::function<void(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len)>;

// Implemented by each LibreTiny tracker hub (BK72xxBLETracker, LN882HBLETracker, …).
class BleProxyHub {
 public:
  virtual ~BleProxyHub() = default;

  // Write the adapter MAC in printable (MSB-first) order into out[0..5] (out[0] = MSB).
  // This is the inverse of the ScanResultCallback mac[] order; see the byte-order contract there.
  virtual void get_proxy_mac(uint8_t out[6]) = 0;

  // Whether the hub's scanner is currently running.
  virtual bool proxy_scan_running() = 0;

  // Whether the hub is scanning actively (false on passive-only hubs, e.g. BK7231N).
  virtual bool proxy_scan_active() = 0;

  // Register the callback the hub invokes for every raw advertisement.
  virtual void set_proxy_scan_result_callback(ScanResultCallback cb) = 0;
};

}  // namespace esphome::bluetooth_proxy

#endif  // USE_LIBRETINY
