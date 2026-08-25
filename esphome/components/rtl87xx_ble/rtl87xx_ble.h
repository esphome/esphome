// Realtek BLE controller (RTL8720C/D): drives the vendor GAP stack. LibreTiny
// links the SDK's BT libraries and this component drives them - the bk72xx_ble
// arrangement. Hardware-verified on AmebaD; AmebaZ2 is compile-only so far.
//
// Bring-up is async and follows the WiFi STA (coexistence needs it), and there
// is no teardown - bte_deinit() crashes - so the stack stays resident.

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_RTL87XX_BLE

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

namespace esphome::rtl87xx_ble {

// Realtek T_GAP_ADV_EVT_TYPE values, pinned to the SDK enum by static_asserts
// in rtl87xx_ble.cpp. ADV_IND and ADV_SCAN_IND are the scannable types.
static constexpr uint8_t ADV_EVENT_TYPE_ADV_IND = 0;
static constexpr uint8_t ADV_EVENT_TYPE_ADV_SCAN_IND = 2;
static constexpr uint8_t ADV_EVENT_TYPE_SCAN_RSP = 4;

// Raw scan-report callback: (bd_addr LSB-first, addr_type, adv_type, rssi,
// data, len). Contract: invoked on the GAP task, and the pointers are valid
// only for the duration of the call - the consumer must copy the payload and
// hand it to its own task (the tracker's SPSC ring). The queue deliberately
// lives with the consumer: this component stays a thin SDK shim, per the
// LibreTiny maintainer's direction that no BLE API lives in the framework.
using raw_adv_callback_t = void (*)(const uint8_t bd_addr[MAC_ADDRESS_SIZE], uint8_t addr_type, uint8_t adv_type,
                                    int8_t rssi, const uint8_t *data, uint8_t len);

class RTL87xxBLE : public Component {
 public:
  void setup() override;
  void dump_config() override;
  // Promotes a bring-up failure detected on the init task into mark_failed()
  // on the main task; disables itself once bring-up settles either way.
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  bool stack_ready() const;
  /// True while bring-up is still blocked on the WiFi STA, which coexistence
  /// needs running before the stack can start.
  bool waiting_for_network() const;
  /// nullptr clears the callback.
  void set_adv_callback(raw_adv_callback_t cb);
  /// Scan timing in milliseconds and mode; a running scan keeps its
  /// parameters until restarted.
  void set_scan_params(uint16_t interval_ms, uint16_t window_ms, bool active);
  /// Start scanning. False when the stack is not ready or the start failed.
  bool scan_start();
  void scan_stop();
  /// True while the stack reports a starting or active scan, read from its own
  /// device state so a controller-side drop shows here.
  bool scan_running() const;
  /// Bluetooth adapter MAC; the name pins the order because the adv callback's
  /// bd_addr is the opposite (LSB-first). Falls back to the WiFi MAC when the
  /// BT eFuse slot is unprogrammed.
  void get_mac_msb_first(uint8_t out[MAC_ADDRESS_SIZE]) const;
};

}  // namespace esphome::rtl87xx_ble

#endif  // USE_RTL87XX_BLE
