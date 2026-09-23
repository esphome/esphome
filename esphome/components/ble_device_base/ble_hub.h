// ble_hub.h
//
// The platform-neutral BLE tracker contract: shared types plus the method
// surface every tracker provides (documented below). Exactly one tracker
// exists per build, so BLEHub is a compile-time alias (ble_hub_impl.h), not
// an abstract interface — no vtable, every hub call inlinable. Consumers
// include ble_hub_impl.h and bind in YAML via cv.use_id(BLEHub).
//
// Chip differences are expressed as data (HubCapabilities), never as
// platform conditionals in consumers.

#pragma once

#include "ble_device.h"
#include "esphome/core/defines.h"

#include <concepts>
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
/// RawAdvertisementCallback, delivered on the ESPHome main loop. Only hubs
/// that push provide the setter; consumers of the rest poll scan_running().
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
  /// bluetooth_connection backend (rp2 binds the BLEGattConnection alias in
  /// bluetooth_connection_gatt_backend.h; esp32 uses its Bluedroid client).
  /// Today: esp32 and rp2.
  bool gatt;
  /// request_scan_mode() is honored at runtime. Distinct from active_scan:
  /// a passive-only controller can never switch, and a hub may support
  /// active scanning yet still refuse the runtime switch (esp32_ble_tracker
  /// drives its mode through its own tracker API).
  bool scan_mode_switch;
};

// The BLEHub method surface, asserted where ble_hub_impl.h binds the alias.
// Semantics beyond the signatures:
// - register_listener: parsed-advertisement consumers (sensors, triggers).
// - set_raw_advertisement_callback: raw stream, one consumer at a time.
// - get_adapter_mac: printable order, out[0] = MSB.
// - scan_active: the current/configured mode sends scan requests.
// - request_scan_mode: false = cannot honor, state untouched (the caller
//   reports the real state back); true = applied immediately, restarting a
//   running scan. Honoring is advertised by HubCapabilities::scan_mode_switch.
// Push hubs additionally provide set_scanner_state_callback(ScannerStateCallback)
// and get_scanner_state() under USE_BLE_SCANNER_STATE_CALLBACK; the concept
// requires both exactly when that define is set. A push hub must emit a
// transition for every accepted or refused mode request - consumers skip
// their own mode report on push builds.
template<typename T>
concept BLEHubContract = requires(T hub, ESPBTDeviceListener *listener, RawAdvertisementCallback raw_callback,
                                  uint8_t *mac) {
  hub.register_listener(listener);
  hub.set_raw_advertisement_callback(raw_callback);
  { T::get_capabilities() } -> std::same_as<HubCapabilities>;
  hub.get_adapter_mac(mac);
  { hub.scan_running() } -> std::same_as<bool>;
  { hub.scan_active() } -> std::same_as<bool>;
  { hub.request_scan_mode(true) } -> std::same_as<bool>;
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  hub.set_scanner_state_callback(ScannerStateCallback{});
  { hub.get_scanner_state() } -> std::same_as<ScannerState>;
#endif
};

}  // namespace esphome::ble_device_base
