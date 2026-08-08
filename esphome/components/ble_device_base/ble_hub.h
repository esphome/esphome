// ble_hub.h
//
// The platform-neutral BLE tracker contract: the types every tracker and
// consumer share, plus (below) the method surface every tracker provides.
//
// Exactly one tracker component exists per build (one chip, one controller),
// so BLEHub is not an abstract interface: ble_hub_impl.h binds the name to
// the build's tracker with a `using` alias, and every hub call is a direct,
// inlinable member call — no vtable, no virtual dispatch. Trackers include
// this header and implement the documented surface; consumers include
// ble_hub_impl.h and bind in YAML via `cv.use_id(BLEHub)` (the Python-side
// class, which resolves whichever tracker the config declares). Adding a new
// BLE chip requires only a new tracker component that provides the surface
// and an alias arm in ble_hub_impl.h.
//
// Chip differences are expressed as data (HubCapabilities), never as
// platform conditionals in consumers.

#pragma once

#include "ble_device.h"

#include <cstdint>

namespace esphome::ble_device_base {

/// One raw advertisement as delivered by the controller — a borrowed view,
/// valid only for the duration of the invoke() callback.
struct RawAdvertisement {
  /// Producers convert their native byte order at the emit site, so no
  /// byte-order convention crosses this contract.
  uint64_t address;
  const uint8_t *data;
  uint16_t data_len;
  int8_t rssi;  // signed dBm
  uint8_t addr_type;
};

/// Subscriber slot for the raw-advertisement stream (the bluetooth_proxy
/// path). The hub delivers on the ESPHome main loop. Same shape as
/// logger.h's LogCallback: an instance pointer plus a plain function
/// pointer — no virtuals, no std::function.
///
/// Usage:
///   hub->set_raw_advertisement_callback({this, [](void *self, const RawAdvertisement &adv) {
///     static_cast<MyComponent *>(self)->on_raw_advertisement(adv);
///   }});
struct RawAdvertisementCallback {
  void *instance{nullptr};
  void (*fn)(void *instance, const RawAdvertisement &adv){nullptr};
  /// A default-constructed slot is "no subscriber"; hubs must guard on this.
  bool is_set() const { return this->fn != nullptr; }
  void invoke(const RawAdvertisement &adv) const { this->fn(this->instance, adv); }
};

/// Scanner lifecycle, wire-value aligned with the api enum so consumers cast
/// directly (pinned by static_asserts at the cast sites).
enum class ScannerState : uint8_t {
  IDLE = 0,
  STARTING = 1,
  RUNNING = 2,
  FAILED = 3,
  STOPPING = 4,
  STOPPED = 5,
};

/// Subscriber slot for scanner-state transitions; same shape as
/// RawAdvertisementCallback, delivered on the ESPHome main loop. Hubs that
/// cannot push drop the registration and the consumer falls back to polling
/// scan_running().
struct ScannerStateCallback {
  void *instance{nullptr};
  void (*fn)(void *instance, ScannerState state){nullptr};
  bool is_set() const { return this->fn != nullptr; }
  void invoke(ScannerState state) const { this->fn(this->instance, state); }
};

/// What a tracker's controller/SDK can do — consumers branch on data, not #ifdefs.
struct HubCapabilities {
  /// Controller can send scan requests (active scanning).
  bool active_scan;
  /// Controller (or tracker) delivers advertisement + scan response as one merged
  /// frame. When false, consumers relying on scan-response fields (e.g. names)
  /// may only see them where the receiver merges per address (Home Assistant does).
  bool merges_scan_response;
  /// GATT client connections are available: the platform has a
  /// bluetooth_connection backend implementing ble_device_base::BLEGattConnection
  /// (ble_gatt_client.h). Today: esp32 and rp2.
  bool gatt;
  /// request_scan_mode() is honored at runtime. Distinct from active_scan:
  /// a passive-only controller (bk72xx) can never switch, and a hub may
  /// support active scanning yet still refuse the runtime switch
  /// (esp32_ble_tracker drives its mode through its own tracker API).
  bool scan_mode_switch;
};

// The BLEHub method surface. Duck-typed: a tracker that misses one of these
// fails to compile at the consumer's call site, not at a class definition.
//
//   /// Register a parsed-advertisement consumer (BLE sensors, automation triggers).
//   void register_listener(ESPBTDeviceListener *listener);
//
//   /// Wire the raw-advertisement stream (bluetooth_proxy). One consumer at a time.
//   void set_raw_advertisement_callback(RawAdvertisementCallback callback);
//
//   /// Push subscriber for scanner-state transitions, invoked where the hub's
//   /// state changes. Provided only by hubs that push (today: esp32), gated on
//   /// USE_BLE_SCANNER_STATE_CALLBACK (bluetooth_proxy emits the define), so
//   /// subscriber-less builds carry no storage. Consumers of a hub without it
//   /// fall back to polling scan_running().
//   void set_scanner_state_callback(ScannerStateCallback callback);
//
//   static constexpr HubCapabilities get_capabilities();
//
//   /// Adapter MAC in printable (MSB-first) order, out[0] = MSB.
//   void get_adapter_mac(uint8_t out[6]);
//
//   bool scan_running();
//   /// True when the current/configured scan mode is active (scan requests sent).
//   bool scan_active();
//   /// Request a scan-mode change (active = send scan requests). Returns false
//   /// when the hub cannot honor the request; the caller reports the real state
//   /// back to its subscriber. A hub that returns true applies the mode
//   /// immediately: a running scan is restarted with the new mode, an idle one
//   /// picks it up on its next start. A hub without a mode switch refuses
//   /// without changing any state. Independent of HubCapabilities::active_scan:
//   /// that bit describes what the CONTROLLER can do; whether this method
//   /// honors requests is advertised by HubCapabilities::scan_mode_switch, so
//   /// consumers can gate features on the switch without probing.
//   bool request_scan_mode(bool active);

}  // namespace esphome::ble_device_base
