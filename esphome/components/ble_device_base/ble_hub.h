// ble_hub.h
//
// BLEHub — the platform-neutral BLE tracker contract.
//
// Every BLE tracker component (esp32_ble_tracker, bk72xx_ble_tracker,
// ln882h_ble_tracker, future chips) implements this interface; every BLE
// consumer (sensor components, bluetooth_proxy) binds to it — in YAML via
// `cv.use_id(BLEHub)`, which resolves whichever tracker the config declares.
// Adding a new BLE chip therefore requires only a new tracker component that
// implements BLEHub: no consumer, registry, or base changes.
//
// Chip differences are expressed as data (HubCapabilities), never as
// platform conditionals in consumers.

#pragma once

#include "ble_device.h"

#include <cstdint>
#include <functional>

namespace esphome::ble_device_base {

/// Callback for raw advertisements (the bluetooth_proxy path).
/// mac[] is least-significant octet first (BLE controller convention);
/// the hub delivers on the ESPHome main loop.
using RawAdvertisementCallback =
    std::function<void(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len)>;

/// What a tracker's controller/SDK can do — consumers branch on data, not #ifdefs.
struct HubCapabilities {
  /// Controller can send scan requests (active scanning).
  bool active_scan;
  /// Controller (or tracker) delivers advertisement + scan response as one merged
  /// frame. When false, consumers relying on scan-response fields (e.g. names)
  /// may only see them where the receiver merges per address (Home Assistant does).
  bool merges_scan_response;
  /// GATT client connections are available (today: esp32 only, but a chip SDK
  /// gaining GATT support only has to flip this bit).
  bool gatt;
};

class BLEHub {
 public:
  virtual ~BLEHub() = default;

  /// Register a parsed-advertisement consumer (BLE sensors, automation triggers).
  virtual void register_listener(ESPBTDeviceListener *listener) = 0;

  /// Wire the raw-advertisement stream (bluetooth_proxy). One consumer at a time.
  virtual void set_raw_advertisement_callback(RawAdvertisementCallback cb) = 0;

  virtual HubCapabilities get_capabilities() const = 0;

  /// Adapter MAC in printable (MSB-first) order, out[0] = MSB.
  virtual void get_adapter_mac(uint8_t out[6]) = 0;

  virtual bool scan_running() = 0;
  /// True when the current/configured scan mode is active (scan requests sent).
  virtual bool scan_active() = 0;
};

}  // namespace esphome::ble_device_base
