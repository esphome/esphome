// bk72xx_ble.cpp
//
// BLE controller support for the BK72xx BLE-5.x chips (LibreTiny beken-72xx
// family) — the platform analog of esp32_ble / rp2040_ble. Owns the Beken BDK
// BLE stack bring-up (ble_entry()) and the controller BLE address. Consumers
// (bk72xx_ble_tracker) build on this component and contain no SDK calls of
// their own.
//
// NOTE: the Beken BDK BLE 5.x stack is compiled and linked by the LibreTiny
// beken-72xx builder itself (prebuilt libble_<chip>.a + ble_5_x sources, gated
// on CFG_SUPPORT_BLE / CFG_BLE_VERSION in sys_config.h). This component only
// calls into it via its public API — no framework patch is required.

#include "bk72xx_ble.h"  // pulls esphome/core/defines.h for USE_BK72XX_BLE

#ifdef USE_BK72XX_BLE

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"  // get_mac_address_raw()
#include "esphome/core/log.h"

// ---------------------------------------------------------------------------
// SDK-capability gate (not a chip allowlist).
// This component drives the Beken BLE *5.x* controller via its public API,
// `ble_api.h`, which the LibreTiny beken-72xx builder ships only for the
// BLE-5.x SoCs (it selects the `ble_pub` 5.x stack from CFG_BLE_VERSION; the
// 4.2 SoCs build a different, older API with no ble_api.h). Gate on the header
// itself so any BLE-5.x Beken chip — present or future — is supported without a
// hard-coded list, and a non-5.x build fails here with a clear message instead
// of a cryptic "ble_api.h: No such file or directory".
// ---------------------------------------------------------------------------
#if defined(CLANG_TIDY)
// The clang-tidy environment does not carry the full Beken BDK BLE 5.x API
// (its ble_api.h variant lacks parts of the 5.x surface), so there is nothing
// accurate to analyze the SDK calls against — skip the file under analysis.
#define BK72XX_BLE_NO_SDK
#elif !__has_include("ble_api.h")
#error \
    "bk72xx_ble requires a BLE 5.x Beken SDK (ble_api.h). Supported SoCs: BK7231N/BK7236 (BLE 5.1) and BK7238/BK7252N/BK7253 (BLE 5.2). BK7231T/BK7251/BK7271 (BLE 4.2) and BK7231Q (no BLE) are not supported."
#endif

#ifndef BK72XX_BLE_NO_SDK

// ---------------------------------------------------------------------------
// Beken BDK BLE 5.x SDK surface used here. Wrapped in extern "C" because these
// are C symbols consumed from C++.
// ---------------------------------------------------------------------------
extern "C" {
#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
#include "common_bt_defines.h"  // struct bd_addr
// The controller's public BLE address, populated by the BDK during ble_entry().
// Present on BK7231N; the other BLE-5.x chips' stacks have no such symbol — there the
// address is derived from the WiFi MAC instead (matching the BDK's own fallback).
extern struct bd_addr common_default_bdaddr;
#endif
// ble_entry() brings up the BDK BLE stack; it is not declared in ble_api.h, so
// declare it here.
void ble_entry(void);
}

namespace esphome::bk72xx_ble {

static const char *const TAG = "bk72xx_ble";

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void BK72xxBLE::setup() {
  // Resolve the MAC early so get_mac_lsb_first() is valid for consumers before
  // the stack is up (it is re-read once ble_entry() has run).
  this->resolve_mac_();
  if (this->enable_on_boot_) {
    this->enable();
  }
}

// AFTER_WIFI, not BLUETOOTH: replicates the proven pre-split timing — the BDK
// is first touched only once WiFi is up (single-core WiFi/BLE bring-up order).
float BK72xxBLE::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void BK72xxBLE::enable() {
  if (this->state_ != BLEComponentState::STATE_OFF)
    return;
  this->state_ = BLEComponentState::ENABLING;

  // One-time BLE stack init. The BDK has no teardown path — init happens at most once.
  ble_entry();

  delay(100);  // NOLINT — one-time BLE stack init; the SDK needs this settle time

  // Re-read the BLE MAC now that the controller is up (common_default_bdaddr is
  // populated by ble_entry()); resolve_mac_() may have fallen back earlier.
  this->resolve_mac_();

#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
  // Liveness heuristic (BK7231N): a healthy ble_entry() populates
  // common_default_bdaddr during init, so all-zero after the settle delay
  // suggests the stack did not come up. The BDK entry point returns void — no
  // return code exists — so warn rather than fail: scan starts against a dead
  // stack already fail cleanly downstream (no idle activity handle).
  bool bdaddr_live = false;
  for (uint8_t b : common_default_bdaddr.addr) {
    if (b != 0) {
      bdaddr_live = true;
      break;
    }
  }
  if (!bdaddr_live)
    ESP_LOGW(TAG, "Controller address still unset after init; BLE stack may not have started");
#endif

  this->state_ = BLEComponentState::ACTIVE;
  ESP_LOGD(TAG, "BLE stack initialised");
}

void BK72xxBLE::get_mac_lsb_first(uint8_t out[6]) const {
  for (int i = 0; i < 6; i++)
    out[i] = this->ble_mac_[i];
}

void BK72xxBLE::dump_config() {
  // ble_mac_ is stored LSB-first (BLE convention); print [5..0] for the
  // MSB-first order Home Assistant shows.
  ESP_LOGCONFIG(TAG,
                "BK72xx BLE:\n"
                "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "  Active: %s",
                this->ble_mac_[5], this->ble_mac_[4], this->ble_mac_[3], this->ble_mac_[2], this->ble_mac_[1],
                this->ble_mac_[0], YESNO(this->is_active()));
}

// ---------------------------------------------------------------------------
// MAC resolution
// ---------------------------------------------------------------------------

void BK72xxBLE::resolve_mac_() {
#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
  // BK7231N: the BDK populates common_default_bdaddr (LSB-first, BLE convention)
  // during ble_entry(). It may still be zero before the stack is up; if so, fall
  // through to the WiFi-derived MAC below.
  bool nonzero = false;
  for (uint8_t b : common_default_bdaddr.addr) {
    if (b != 0) {
      nonzero = true;
      break;
    }
  }
  if (nonzero) {
    memcpy(this->ble_mac_, common_default_bdaddr.addr, 6);
    return;
  }
#endif
  // Chips whose BLE stack does not export common_default_bdaddr (BK7238 and the other
  // BLE-5.x SoCs), or BK7231N before the stack is up: derive the BLE MAC exactly as the
  // Beken BDK does in bdaddr_env_init() — the WiFi STA MAC with only its last byte
  // incremented (sta_mac[5] += 1, a plain byte increment with no carry into the next
  // byte), OUI unchanged. This reproduces the address the controller advertises with
  // (verified against the BK7231N BLE-5.1 and BK7252N/BK7238 BLE-5.2 SDK sources), so it
  // matches on every device, including the last-byte == 0xFF edge that a 24-bit increment
  // would carry differently.
  uint8_t wifi_mac[6];
  get_mac_address_raw(wifi_mac);  // MSB-first
  const uint8_t ble[6] = {wifi_mac[0], wifi_mac[1], wifi_mac[2],
                          wifi_mac[3], wifi_mac[4], static_cast<uint8_t>(wifi_mac[5] + 1)};
  // Store LSB-first to match the BLE controller's address ordering.
  for (int i = 0; i < 6; i++)
    this->ble_mac_[i] = ble[5 - i];
}

}  // namespace esphome::bk72xx_ble

#endif  // BK72XX_BLE_NO_SDK
#endif  // USE_BK72XX_BLE
