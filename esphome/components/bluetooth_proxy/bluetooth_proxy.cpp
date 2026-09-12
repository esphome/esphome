#include "bluetooth_proxy.h"

#ifdef USE_BLUETOOTH_PROXY

#include "esphome/components/api/api_server.h"
#ifdef USE_BLUETOOTH_PROXY_FILTERING
#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include <cctype>
#endif  // USE_BLUETOOTH_PROXY_FILTERING
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"
#include "esphome/core/application.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <limits>

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

// BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE is defined during code generation
// It sets the batch size for BLE advertisements to maximize WiFi efficiency

// Verify BLE advertisement data array size matches the BLE specification (31 bytes adv + 31 bytes scan response)
static_assert(sizeof(((api::BluetoothLERawAdvertisement *) nullptr)->data) == 62,
              "BLE advertisement data array size mismatch");

BluetoothProxy::BluetoothProxy() { global_bluetooth_proxy = this; }

// The neutral enum's values are the wire values.
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::IDLE) == api::enums::BLUETOOTH_SCANNER_STATE_IDLE);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STARTING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STARTING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::RUNNING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_RUNNING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::FAILED) ==
              api::enums::BLUETOOTH_SCANNER_STATE_FAILED);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STOPPING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STOPPING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STOPPED) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STOPPED);

bool BluetoothProxy::send_bluetooth_scanner_state_(ble_device_base::ScannerState state) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing owed
  api::BluetoothScannerStateResponse resp;
  resp.state = static_cast<api::enums::BluetoothScannerState>(state);
  resp.mode = this->hub_->scan_active() ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                                        : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  resp.configured_mode = this->configured_scan_active_
                             ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                             : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  return this->api_connection_->send_message(resp);
}

#ifdef USE_BLE_SCANNER_STATE_CALLBACK
void BluetoothProxy::send_scanner_state_(ble_device_base::ScannerState state) {
  // False only on a refused frame, so the latch arms only when a retry is owed.
  this->scanner_state_pending_ = !this->send_bluetooth_scanner_state_(state);
}
#else
void BluetoothProxy::send_polled_scanner_state_() {
  // One read feeds both the frame and the change detector; the detector only
  // advances if the frame was accepted, so a dropped send (WOULD_BLOCK on a
  // full TX buffer) is retried from loop() instead of leaving a stale state.
  const bool running = this->hub_->scan_running();
  if (this->send_bluetooth_scanner_state_(running ? ble_device_base::ScannerState::RUNNING
                                                  : ble_device_base::ScannerState::IDLE)) {
    this->last_scan_running_ = running;
  }
}
#endif  // USE_BLE_SCANNER_STATE_CALLBACK

void BluetoothProxy::setup() {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->connections_free_response_.limit = BLUETOOTH_PROXY_MAX_CONNECTIONS;
  this->connections_free_response_.free = BLUETOOTH_PROXY_MAX_CONNECTIONS;
#endif

#ifdef USE_BLUETOOTH_PROXY_FILTERING
  // Unpack the compile-time IRK blob (concatenated 32-hex-char keys) once, so
  // the advertisement path only ever touches the parsed vector.
  if (this->irks_hex_ != nullptr) {
    const size_t len = strlen(this->irks_hex_);
    this->irks_.reserve(len / 32);
    for (size_t i = 0; i + 32 <= len; i += 32) {
      std::array<uint8_t, 16> irk{};
      // Validation already guaranteed 32 hex chars per key; skip anything that
      // somehow fails to parse rather than installing a half-filled key.
      if (parse_hex(this->irks_hex_ + i, 32, irk.data(), 16) == 32)
        this->irks_.push_back(irk);
    }
    this->irks_hex_ = nullptr;
    ESP_LOGCONFIG(TAG, "Loaded %u IRK(s); unmatched RPAs will be dropped", static_cast<unsigned>(this->irks_.size()));
  }

  // Same pattern as the IRK blob: parse the compile-time hex once, then drop it
  // so the advertisement path only ever touches the parsed vector.
  if (!this->service_uuid128_hex_.empty()) {
    this->service_uuid128_.reserve(this->service_uuid128_hex_.size());
    for (const char *hex : this->service_uuid128_hex_) {
      std::array<uint8_t, 16> uuid{};
      // Validation already guaranteed 32 hex chars; skip anything that somehow
      // fails to parse rather than installing a half-filled UUID.
      if (parse_hex(hex, 32, uuid.data(), 16) == 32)
        this->service_uuid128_.push_back(uuid);
    }
    this->service_uuid128_hex_.clear();
    this->service_uuid128_hex_.shrink_to_fit();
  }

  if (!this->service_uuid_allowlist_.empty() || !this->service_uuid128_.empty()) {
    ESP_LOGCONFIG(TAG, "Service UUID allowlist active (%u short, %u long); matching adverts bypass every filter",
                  static_cast<unsigned>(this->service_uuid_allowlist_.size()),
                  static_cast<unsigned>(this->service_uuid128_.size()));
    for (const uint16_t uuid : this->service_uuid_allowlist_)
      ESP_LOGCONFIG(TAG, "  allowing service UUID 0x%04X", uuid);
    for (const auto &uuid : this->service_uuid128_)
      ESP_LOGCONFIG(TAG, "  allowing service UUID %02X%02X%02X%02X-...-%02X%02X%02X%02X", uuid[0], uuid[1], uuid[2],
                    uuid[3], uuid[12], uuid[13], uuid[14], uuid[15]);
  }
#endif  // USE_BLUETOOTH_PROXY_FILTERING

  // Capture the configured scan mode from YAML before any API changes
  this->configured_scan_active_ = this->hub_->scan_active();

  this->hub_->set_raw_advertisement_callback({this, [](void *self, const ble_device_base::RawAdvertisement &adv) {
                                                static_cast<BluetoothProxy *>(self)->on_raw_advertisement_(adv);
                                              }});
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Only push hubs compile the slot; elsewhere loop() polls scan_running().
  this->hub_->set_scanner_state_callback({this, [](void *self, ble_device_base::ScannerState state) {
                                            static_cast<BluetoothProxy *>(self)->send_scanner_state_(state);
                                          }});
#endif
}

// The hub delivers raw advertisements on the ESPHome main loop.
#ifdef USE_BLUETOOTH_PROXY_FILTERING

static const uint32_t ESPRESSIF_OUIS[] = {
    0x004B12, 0x007007, 0x00FB4A, 0x048308, 0x04B247, 0x083A8D, 0x083AF2, 0x089272, 0x08A6F7, 0x08AD0A, 0x08B61F,
    0x08D1F9, 0x08F9E0, 0x0C4EA0, 0x0C8B95, 0x0CB815, 0x0CDC7E, 0x10003B, 0x10061C, 0x1020BA, 0x1051DB, 0x10521C,
    0x1091A8, 0x1097BD, 0x10B41D, 0x10BDA3, 0x140808, 0x142B2F, 0x14335C, 0x146393, 0x14C19F, 0x188B0E, 0x18FE34,
    0x1C2904, 0x1C6920, 0x1C8B84, 0x1C8F57, 0x1C9DC2, 0x1CC3AB, 0x1CDBD4, 0x1CE4CB, 0x202565, 0x2043A8, 0x20500D,
    0x206EF1, 0x209BA9, 0x20D5C2, 0x20E7C8, 0x240AC4, 0x244CAB, 0x24587C, 0x2462AB, 0x246F28, 0x24A160, 0x24B2DE,
    0x24D7EB, 0x24DCC3, 0x24EC4A, 0x2805A5, 0x28372F, 0x28562F, 0x288485, 0x2C3AE8, 0x2CBCBB, 0x2CF432, 0x3030F9,
    0x3076F5, 0x308398, 0x30AEA4, 0x30C6F7, 0x30C922, 0x30EDA0, 0x345F45, 0x348518, 0x34865D, 0x349454, 0x34987A,
    0x34AB95, 0x34B472, 0x34B7DA, 0x34CDB0, 0x38182B, 0x383E51, 0x3844BE, 0x3C0D0D, 0x3C0F02, 0x3C6105, 0x3C71BF,
    0x3C8427, 0x3C8A1F, 0x3CDC75, 0x3CE90E, 0x4022D8, 0x404CCA, 0x409151, 0x40F520, 0x441793, 0x441BF6, 0x441D64,
    0x447B30, 0x44B176, 0x44BD8D, 0x4827E2, 0x4831B7, 0x483FDA, 0x485519, 0x489D31, 0x48AFF3, 0x48CA43, 0x48E729,
    0x48F6EE, 0x4C11AE, 0x4C7525, 0x4CC382, 0x4CEBD6, 0x500291, 0x50787D, 0x543204, 0x5443B2, 0x545AA6, 0x549DEA,
    0x582ABD, 0x588C81, 0x58BF25, 0x58CF79, 0x58E6C5, 0x5C013B, 0x5CCF7F, 0x600194, 0x6055F9, 0x648914, 0x64B708,
    0x64E833, 0x680947, 0x6825DD, 0x686725, 0x689DD2, 0x68B6B3, 0x68C63A, 0x68EE8F, 0x68FE71, 0x6C3DD8, 0x6CB456,
    0x6CC840, 0x70039F, 0x70041D, 0x704BCA, 0x70AF09, 0x70B8F6, 0x744DBD, 0x781C3C, 0x782184, 0x78421C, 0x78E36D,
    0x78EE4C, 0x7C0C5F, 0x7C2C67, 0x7C4FAD, 0x7C7398, 0x7C87CE, 0x7C9EBD, 0x7CD544, 0x7CDFA1, 0x7CE8B1, 0x80456B,
    0x8053E0, 0x80646F, 0x806599, 0x807D3A, 0x80B54E, 0x80F1B2, 0x80F3DA, 0x840D8E, 0x841FE8, 0x84C7BB, 0x84CCA8,
    0x84F3EB, 0x84F703, 0x84FCE6, 0x8813BF, 0x8856A6, 0x885721, 0x88F155, 0x8C4B14, 0x8C4F00, 0x8C8C29, 0x8C94DF,
    0x8CAAB5, 0x8CBFEA, 0x8CCE4E, 0x8CFD49, 0x901506, 0x90380C, 0x90649B, 0x907069, 0x9097D5, 0x90B339, 0x90DA72,
    0x90E5B1, 0x943CC6, 0x9451DC, 0x9454C5, 0x94A990, 0x94B555, 0x94B97E, 0x94E686, 0x983DAE, 0x9888E0, 0x98A316,
    0x98C377, 0x98CDAC, 0x98F4AB, 0x9C139E, 0x9C96D5, 0x9C9C1F, 0x9C9E6E, 0x9CCC01, 0xA020A6, 0xA0764E, 0xA085E3,
    0xA0A3B3, 0xA0B765, 0xA0DD6C, 0xA0F262, 0xA47B9D, 0xA4CB8F, 0xA4CF12, 0xA4E57C, 0xA4F00F, 0xA8032A, 0xA842E3,
    0xA84674, 0xA848FA, 0xA8FD07, 0xAC0BFB, 0xAC1518, 0xAC276E, 0xAC67B2, 0xACA704, 0xACD074, 0xACEBE6, 0xB03FD3,
    0xB08184, 0xB0A604, 0xB0A732, 0xB0B21C, 0xB0CBD8, 0xB43A45, 0xB48A0A, 0xB4A64A, 0xB4BFE9, 0xB4E62D, 0xB81F3F,
    0xB87B4D, 0xB8BB11, 0xB8D61A, 0xB8F009, 0xB8F862, 0xBCDDC2, 0xBCFF4D, 0xC049EF, 0xC04E30, 0xC05D89, 0xC0CDD6,
    0xC44F33, 0xC45BBE, 0xC49E7E, 0xC4D8D5, 0xC4DD57, 0xC4DEE2, 0xC82B96, 0xC82E18, 0xC88541, 0xC88A7B, 0xC8C9A3,
    0xC8DA29, 0xC8F09E, 0xCC50E3, 0xCC68C7, 0xCC7B5C, 0xCC7E1F, 0xCC8DA2, 0xCCBA97, 0xCCDBA7, 0xD09AAF, 0xD0CF13,
    0xD0EF76, 0xD40592, 0xD48AFC, 0xD48C49, 0xD4D4DA, 0xD4E9F4, 0xD4F98D, 0xD8132A, 0xD83BDA, 0xD885AC, 0xD8A01D,
    0xD8BC38, 0xD8BFC0, 0xD8F15B, 0xDC0675, 0xDC0A69, 0xDC1ED5, 0xDC4F22, 0xDC5475, 0xDC55B1, 0xDCB4D9, 0xDCDA0C,
    0xE05A1B, 0xE072A1, 0xE08CFE, 0xE09806, 0xE0E2E6, 0xE465B8, 0xE4B063, 0xE4B323, 0xE80690, 0xE831CD, 0xE83DC1,
    0xE868E7, 0xE86BEA, 0xE89F6D, 0xE8DB84, 0xE8F60A, 0xEC6260, 0xEC64C9, 0xEC94CB, 0xECC9FF, 0xECD61B, 0xECDA3B,
    0xECE334, 0xECFABC, 0xF008D1, 0xF0161D, 0xF024F9, 0xF09E9E, 0xF0F5BD, 0xF412FA, 0xF42DC9, 0xF4650B, 0xF4CFA2,
    0xF843EE, 0xF85B1B, 0xF8B3B7, 0xFC012C, 0xFCB467, 0xFCE8C0, 0xFCF5C4,
};
static constexpr size_t ESPRESSIF_OUI_COUNT = sizeof(ESPRESSIF_OUIS) / sizeof(ESPRESSIF_OUIS[0]);

bool BluetoothProxy::is_espressif_oui_(uint64_t addr) {
  const uint32_t oui = static_cast<uint32_t>((addr >> 24) & 0xFFFFFF);
  size_t lo = 0, hi = ESPRESSIF_OUI_COUNT;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (ESPRESSIF_OUIS[mid] < oui) {
      lo = mid + 1;
    } else if (ESPRESSIF_OUIS[mid] > oui) {
      hi = mid;
    } else {
      return true;
    }
  }
  return false;
}

bool BluetoothProxy::payload_blocked_(const uint8_t *data, uint16_t len) const {
  // AD structures are [length][type][payload...], length covering type+payload.
  // One pass checks both the local name and the manufacturer id so a dropped
  // advertisement is never walked twice.
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0)
      break;  // Zero length terminates the payload.
    // Reject a structure that claims to run past the buffer (truncated packet).
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    const uint8_t type = data[i + 1];
    // 0xFF manufacturer specific data: first two payload bytes are the
    // Bluetooth SIG company identifier, little-endian.
    if (type == 0xFF && field_len >= 3) {
      const uint16_t company = static_cast<uint16_t>(data[i + 2]) | (static_cast<uint16_t>(data[i + 3]) << 8);
      // HomeKit accessories advertise under Apple's company id with subtype
      // 0x06. Blocklisting Apple to kill phone/AirPods/AirTag noise would take
      // them with it, and they carry no IRK to rescue them, so exempt HAP
      // explicitly rather than forcing users to choose between the two.
      const bool is_hap = this->allow_homekit_ && company == 0x004C && field_len >= 4 && data[i + 4] == 0x06;
      if (!is_hap) {
        for (const uint16_t blocked : this->manufacturer_blocklist_) {
          if (blocked == company)
            return true;
        }
      }
    }
    // 0x09 complete local name, 0x08 shortened local name.
    if ((type == 0x09 || type == 0x08) && field_len > 1) {
      const char *name = reinterpret_cast<const char *>(&data[i + 2]);
      const uint8_t name_len = field_len - 1;
      for (const char *needle : this->name_blocklist_) {
        const size_t needle_len = strlen(needle);
        if (needle_len == 0 || needle_len > name_len)
          continue;
        // Case-insensitive substring search; the name is not NUL-terminated,
        // so this walks it by length rather than using strstr().
        for (uint8_t start = 0; start + needle_len <= name_len; start++) {
          size_t k = 0;
          while (k < needle_len && static_cast<char>(tolower(static_cast<unsigned char>(name[start + k]))) == needle[k])
            k++;
          if (k == needle_len)
            return true;
        }
      }
    }
    i += field_len + 1;
  }
  return false;
}

// The Bluetooth Base UUID, big-endian, with the 16-bit slot (bytes 2-3) zeroed.
// A SIG-allocated short UUID advertised in 128-bit form is this with those two
// bytes filled in, so a device using the long form of 0xFFF6 still matches a
// 16-bit allowlist entry.
static const uint8_t BT_BASE_UUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                         0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

bool BluetoothProxy::uuid128_matches_(const uint8_t *le_bytes) const {
  // Advertisements carry 128-bit UUIDs little-endian; flip to canonical order
  // once, then compare.
  uint8_t be[16];
  for (uint8_t k = 0; k < 16; k++)
    be[k] = le_bytes[15 - k];

  for (const auto &allowed : this->service_uuid128_) {
    if (memcmp(be, allowed.data(), 16) == 0)
      return true;
  }

  // Long form of a SIG short UUID: everything but bytes 2-3 matches the base.
  if (!this->service_uuid_allowlist_.empty() && memcmp(be, BT_BASE_UUID, 2) == 0 &&
      memcmp(be + 4, BT_BASE_UUID + 4, 12) == 0) {
    const uint16_t shortened = static_cast<uint16_t>(be[3]) | (static_cast<uint16_t>(be[2]) << 8);
    for (const uint16_t allowed : this->service_uuid_allowlist_) {
      if (allowed == shortened)
        return true;
    }
  }
  return false;
}

bool BluetoothProxy::payload_has_allowed_service_uuid_(const uint8_t *data, uint16_t len) const {
  // Same [length][type][payload...] walk as payload_blocked_. A service UUID can
  // appear in several places and a device in pairing mode does not consistently
  // use one, so every form is checked:
  //   0x02/0x03 incomplete/complete 16-bit UUID list   (n * 2 bytes)
  //   0x14      16-bit solicitation list               (n * 2 bytes)
  //   0x16      service data, 16-bit UUID              (2 bytes + data)
  //   0x06/0x07 incomplete/complete 128-bit UUID list  (n * 16 bytes)
  //   0x15      128-bit solicitation list              (n * 16 bytes)
  //   0x21      service data, 128-bit UUID             (16 bytes + data)
  const bool have16 = !this->service_uuid_allowlist_.empty();
  const bool have128 = !this->service_uuid128_.empty();
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0)
      break;  // Zero length terminates the payload.
    // Reject a structure that claims to run past the buffer (truncated packet).
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    const uint8_t type = data[i + 1];
    // Payload is the field minus its type byte. The list types carry a packed
    // array; the service-data types carry exactly one UUID then opaque bytes,
    // so those stop after the first entry.
    const uint8_t payload_len = field_len - 1;

    if (have16 && (type == 0x02 || type == 0x03 || type == 0x14 || type == 0x16)) {
      const uint8_t entries = payload_len / 2;
      const uint8_t limit = (type == 0x16) ? (entries > 0 ? 1 : 0) : entries;
      for (uint8_t p = 0; p < limit; p++) {
        const uint16_t uuid =
            static_cast<uint16_t>(data[i + 2 + p * 2]) | (static_cast<uint16_t>(data[i + 3 + p * 2]) << 8);
        for (const uint16_t allowed : this->service_uuid_allowlist_) {
          if (allowed == uuid)
            return true;
        }
      }
    }

    // The long form can match a 16-bit entry via the base UUID, so this arm runs
    // whenever either list is configured.
    if ((have128 || have16) && (type == 0x06 || type == 0x07 || type == 0x15 || type == 0x21)) {
      const uint8_t entries = payload_len / 16;
      const uint8_t limit = (type == 0x21) ? (entries > 0 ? 1 : 0) : entries;
      for (uint8_t p = 0; p < limit; p++) {
        if (this->uuid128_matches_(&data[i + 2 + p * 16]))
          return true;
      }
    }

    i += field_len + 1;
  }
  return false;
}

bool BluetoothProxy::address_is_rpa_(uint64_t addr, uint8_t addr_type) {
  // addr_type 0 is public; a public address is never resolvable no matter what
  // its top bits look like.
  if (addr_type == 0)
    return false;
  return (((addr >> 40) & 0xC0) == 0x40);
}

bool BluetoothProxy::address_is_non_resolvable_(uint64_t addr, uint8_t addr_type) {
  // Same guard as address_is_rpa_: a public address is never a private one, no
  // matter what its leading bits look like.
  if (addr_type == 0)
    return false;
  return (((addr >> 40) & 0xC0) == 0x00);
}

bool BluetoothProxy::irk_matches_(uint64_t addr) const {
  // Bluetooth Core "ah": hash = e(IRK, 0-padding | prand)[low 24 bits], where
  // the RPA is prand (top 3 bytes) | hash (bottom 3 bytes).
  uint8_t plaintext[16] = {0};
  uint8_t ciphertext[16];
  plaintext[13] = (addr >> 40) & 0xff;
  plaintext[14] = (addr >> 32) & 0xff;
  plaintext[15] = (addr >> 24) & 0xff;
  for (const auto &irk : this->irks_) {
    ble_device_base::aes128_encrypt_block(irk.data(), plaintext, ciphertext);
    if (ciphertext[15] == (addr & 0xff) && ciphertext[14] == ((addr >> 8) & 0xff) &&
        ciphertext[13] == ((addr >> 16) & 0xff))
      return true;
  }
  return false;
}
#endif  // USE_BLUETOOTH_PROXY_FILTERING

void BluetoothProxy::on_raw_advertisement_(const ble_device_base::RawAdvertisement &raw) {
  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr)
    return;

#ifdef USE_BLUETOOTH_PROXY_FILTERING
  // This is the only point where a packet can be suppressed without it crossing
  // the network, so every filter lives here rather than downstream.

  // Explicitly protected addresses bypass every filter, including the RSSI
  // threshold: these are the tracked tags, and a tag being far from *this*
  // proxy is exactly the reading the tracker needs to place it near another.
  // Cheap enough to run first - the list is a handful of entries.
  bool protected_addr = false;
  for (const uint64_t allowed : this->mac_allowlist_) {
    if (allowed == raw.address) {
      protected_addr = true;
      break;
    }
  }

  // A device advertising an allowlisted service UUID is protected exactly like
  // an allowlisted MAC. This has to run here, ahead of the address-type tests,
  // because the case it exists for is a device in pairing mode advertising from
  // a rotating private address: by the time those tests run the advertisement is
  // already gone, and its address could not have been allowlisted in advance.
  //
  // It is deliberately placed before the RSSI test as well. A device being
  // paired is normally close by, but a pairing window is short and
  // user-initiated, so a missed advertisement costs a retry while the extra
  // traffic lasts only as long as the pairing does.
  //
  // Cost: this walks the payload, which the filters below otherwise defer to
  // last. Guarded on a non-empty allowlist so a build that does not use the
  // option keeps the original ordering and pays nothing.
  if (!protected_addr && (!this->service_uuid_allowlist_.empty() || !this->service_uuid128_.empty()) &&
      this->payload_has_allowed_service_uuid_(raw.data, raw.data_len)) {
    protected_addr = true;
    this->adv_allowed_service_uuid_++;
    ESP_LOGVV(TAG, "Allowing packet from %012" PRIX64 ": allowlisted service UUID", raw.address);
  }

  // Distance first, and it applies to everything else: a far-away device is not
  // worth forwarding even when we would otherwise allow it through below.
  // Cheapest test too, so nothing distant ever reaches the AES.
  if (!protected_addr && raw.rssi < this->rssi_threshold_) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": RSSI %d dB below threshold %d dB", raw.address, raw.rssi,
              this->rssi_threshold_);
    return;
  }

  // A non-resolvable private address rotates and carries no identity, so it can
  // never be matched to a device - not even with an IRK. Nothing can be done
  // with these, and each rotation looks like a brand new device downstream.
  if (!protected_addr && this->drop_non_resolvable_ && this->address_is_non_resolvable_(raw.address, raw.addr_type)) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": non-resolvable private address", raw.address);
    return;
  }

  // A Resolvable Private Address we cannot resolve belongs to somebody else's
  // phone or watch: it rotates, so it can never be tracked here and is pure
  // noise. Devices with fixed addresses are left alone - they have no IRK.
  //
  // allow_espressif exempts our own ESPs from this test. Note it is a safety
  // net, not load-bearing: ESPHome advertises on the public MAC, and a public
  // address is never an RPA, so those advertisements do not reach this test
  // anyway. It only bites if an ESP is ever configured to advertise randomly.
  if (!protected_addr && !this->irks_.empty() && this->address_is_rpa_(raw.address, raw.addr_type) &&
      !(this->allow_espressif_ && this->is_espressif_oui_(raw.address))) {
    if (this->irk_matches_(raw.address)) {
      // One of ours. Mark it protected so the payload filters below cannot
      // discard it - our phones and watches advertise Apple manufacturer data,
      // which a manufacturer_blocklist entry would otherwise match.
      protected_addr = true;
    } else {
      this->adv_dropped_++;
      this->adv_dropped_rpa_++;
      ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": unresolved RPA", raw.address);
      return;
    }
  }

  // Payload-based filters last: this is the only test that has to walk the
  // advertisement, and by here most traffic has already been rejected.
  if (!protected_addr && (!this->name_blocklist_.empty() || !this->manufacturer_blocklist_.empty()) &&
      this->payload_blocked_(raw.data, raw.data_len)) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": blocklisted name or manufacturer", raw.address);
    return;
  }
  this->adv_forwarded_++;
#endif  // USE_BLUETOOTH_PROXY_FILTERING

  auto &adv = this->response_.advertisements[this->response_.advertisements_len];
  adv.address = raw.address;
  adv.rssi = raw.rssi;
  adv.address_type = raw.addr_type;
  uint8_t length = raw.data_len > sizeof(adv.data) ? sizeof(adv.data) : static_cast<uint8_t>(raw.data_len);
  adv.data_len = length;
  std::memcpy(adv.data, raw.data, length);

  this->response_.advertisements_len++;

  ESP_LOGVV(TAG, "Queuing raw packet from %012" PRIX64 ", length %d. RSSI: %d dB", raw.address, length, raw.rssi);

  // Flush if we have reached BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE
  if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE) {
    this->flush_pending_advertisements_();
  }
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
void BluetoothProxy::log_connection_request_ignored_(BluetoothConnection *connection, ClientState state) {
  ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, state: %s", connection->get_connection_index(),
           connection->address_str(), ble_device_base::client_state_to_string(state));
}

void BluetoothProxy::log_connection_info_(BluetoothConnection *connection, const char *message) {
  ESP_LOGI(TAG, "[%d] [%s] Connecting %s", connection->get_connection_index(), connection->address_str(), message);
}
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
void BluetoothProxy::log_reply_dropped_(const char *what, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " dropped, TCP buffer full", what, address);
}

void BluetoothProxy::log_reply_deferred_(const char *what, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " deferred, TCP buffer full", what, address);
}

void BluetoothProxy::log_reply_displaced_(const char *what, uint64_t owed, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " dropped, displaced by %012" PRIX64, what, owed, address);
}

void BluetoothProxy::log_not_connected_gatt_(const char *action, const char *type) {
  ESP_LOGW(TAG, "Cannot %s GATT %s, not connected", action, type);
}

void BluetoothProxy::handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action,
                                                const char *type) {
  this->log_not_connected_gatt_(action, type);
  if (!this->send_gatt_error(address, handle, GATT_NOT_CONNECTED)) {
    // No connection, so nothing to latch against; the client's timeout arbitrates.
    this->log_reply_dropped_("Not-connected", address);
  }
}
#endif

void BluetoothProxy::log_advertisement_flush_(bool sent) {
  if (sent) {
    // VV: one line per flush drowns a verbose log in any busy environment.
    ESP_LOGVV(TAG, "Sent batch of %u BLE advertisements", this->response_.advertisements_len);
  } else {
    // The rare congestion signal stays at V.
    ESP_LOGV(TAG, "Batch of %u BLE advertisements dropped, TCP buffer full", this->response_.advertisements_len);
  }
}

void BluetoothProxy::dump_config() {
  // Print configured facts. dump_config runs right after setup, before the
  // radio is up, so live scan state would always read "stopped" here — the
  // loop's BluetoothScannerStateResponse carries the changing value instead.
  char mac_str[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  this->get_bluetooth_mac_address_pretty(mac_str);
  const char *mac_out = mac_str[0] != '\0' ? mac_str : "unavailable (adapter not up yet)";
  const char *scan_mode = this->configured_scan_active_ ? "active" : "passive";
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Active: %s\n"
                "  Connections: %d\n"
                "  Configured scan: %s\n"
                "  Adapter MAC: %s",
                YESNO(this->active_), this->connection_count_, scan_mode, mac_out);
#else
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Mode: advertisement-only (no GATT connections)\n"
                "  Configured scan: %s\n"
                "  Adapter MAC: %s",
                scan_mode, mac_out);
#endif
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

void BluetoothProxy::register_connection(BluetoothConnection *connection) {
  if (this->connection_count_ >= BLUETOOTH_PROXY_MAX_CONNECTIONS) {
    // Cannot happen with codegen-sized registration; a silent drop would
    // surface later as a null proxy_ dereference, so refuse loudly.
    ESP_LOGE(TAG, "Connection registry full, dropping registration");
    return;
  }
  // The hub wrapper has no Component lifecycle, so the index is assigned here.
  connection->connection_index_ = this->connection_count_;
  this->connections_[this->connection_count_++] = connection;
  connection->proxy_ = this;
}

void BluetoothProxy::log_slot_accounting_mismatch_() { ESP_LOGW(TAG, "Connection slot free-count mismatch, clamped"); }

void BluetoothProxy::replace_allocated_slot_(uint64_t find_value, uint64_t set_value) {
  for (auto &slot : this->connections_free_response_.allocated) {
    if (slot == find_value) {
      slot = set_value;
      return;
    }
  }
  // The accounting arrays are only mutated here and sized to the slot count,
  // so a miss means the bookkeeping already drifted — say so.
  ESP_LOGW(TAG, "Connection slot accounting mismatch (find 0x%llx)", (unsigned long long) find_value);
}

void BluetoothProxy::latch_pending_disconnection_(uint64_t address, conn_err_t error) {
  // Match before free entry so one address never occupies two pool slots.
  PendingReply *free_entry = nullptr;
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto &owed = this->pending_disconnections_[i];
    if (owed.matches(address)) {
      owed.set(address, error);
      return;
    }
    if (free_entry == nullptr && owed.empty()) {
      free_entry = &owed;
    }
  }
  if (free_entry != nullptr) {
    this->log_reply_deferred_("Disconnect", address);
    free_entry->set(address, error);
    return;
  }
  // Every entry is owed: evict the first so the newest loss is not silent too.
  this->log_reply_displaced_("Disconnect", this->pending_disconnections_[0].address(), address);
  this->pending_disconnections_[0].set(address, error);
}

void BluetoothProxy::clear_pending_disconnection_(uint64_t address) {
  // A reconnect supersedes the owed disconnect; a late resend would shadow
  // the new connection.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    if (this->pending_disconnections_[i].matches(address)) {
      this->pending_disconnections_[i].clear();
      return;  // latch_pending_disconnection_ keeps at most one entry per address
    }
  }
}

void BluetoothProxy::answer_device_disconnected_(uint64_t address) {
  if (this->send_device_connection(address, false)) {
    // A landed answer satisfies any owed notification for the address; a
    // drained duplicate would follow it otherwise.
    this->clear_pending_disconnection_(address);
    return;
  }
  // Not latched: the client's own request timeout arbitrates, and pooling
  // these would let a request retry loop displace an unsolicited disconnect.
  this->log_reply_dropped_("Disconnect", address);
}

void BluetoothProxy::send_device_disconnected_(uint64_t address, conn_err_t error) {
  if (this->send_device_connection(address, false, 0, error)) {
    // A later disconnect landing for an address that still has one owed would
    // otherwise have the drain repeat it.
    this->clear_pending_disconnection_(address);
    return;
  }
  // A dropped disconnect leaves the client believing the link is live, so
  // every GATT operation on it times out until something else corrects it.
  // latch_pending_disconnection_() reports the leading edge.
  this->latch_pending_disconnection_(address, error);
}

void BluetoothProxy::reset_connection_slot_(BluetoothConnection *connection, conn_err_t reason) {
  // The client has no other way to learn of an unsolicited disconnect.
  this->send_device_disconnected_(connection->get_address(), reason);
  connection->set_address(0);
  connection->send_service_ = INIT_SENDING_SERVICES;
  this->send_connections_free();
}

BluetoothConnection *BluetoothProxy::get_connection_(uint64_t address, bool reserve) {
  // Finish the scan before reserving: a free slot earlier in the array must
  // not win over a later slot that already holds the address, or one device
  // ends up on two slots with a second connection attempt racing the first.
  BluetoothConnection *free_slot = nullptr;
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto *connection = this->connections_[i];
    uint64_t conn_addr = connection->get_address();

    if (conn_addr == address) {
      // A connect request supersedes an owed disconnect.
      if (reserve) {
        this->clear_pending_disconnection_(address);
      }
      return connection;
    }

    if (free_slot == nullptr && conn_addr == 0)
      free_slot = connection;
  }
  if (!reserve || free_slot == nullptr)
    return nullptr;
  this->clear_pending_disconnection_(address);
  free_slot->send_service_ = INIT_SENDING_SERVICES;
  free_slot->set_address(address);
  // All connections must start at INIT
  // We only set the state if we allocate the connection
  // to avoid a race where multiple connection attempts
  // are made.
  free_slot->set_state(ClientState::INIT);
  return free_slot;
}

void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE: {
      auto *connection = this->get_connection_(msg.address, true);
      if (connection == nullptr) {
        ESP_LOGW(TAG, "No free connections available");
        this->answer_device_disconnected_(msg.address);
        return;
      }
      if (!msg.has_address_type) {
        ESP_LOGE(TAG, "[%d] [%s] Missing address type in connect request", connection->get_connection_index(),
                 connection->address_str());
        this->answer_device_disconnected_(msg.address);
        return;
      }
      if (connection->state() == ClientState::CONNECTED || connection->state() == ClientState::ESTABLISHED) {
        this->log_connection_request_ignored_(connection, connection->state());
        connection->send_connected_reply_();
        this->send_connections_free();
        return;
      } else if (connection->state() == ClientState::DISCONNECTING && connection->cancel_teardown()) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request while pending disconnect, cancelling pending disconnect",
                 connection->get_connection_index(), connection->address_str());
        return;
      } else if (connection->state() != ClientState::INIT) {
        // Covers CONNECTING too: a repeat request during a connect attempt is
        // ignored the same way.
        this->log_connection_request_ignored_(connection, connection->state());
        return;
      }
      if (msg.request_type == api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE) {
        connection->set_connection_type(ble_device_base::ConnectionType::V3_WITH_CACHE);
        this->log_connection_info_(connection, "v3 with cache");
      } else {  // BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE
        connection->set_connection_type(ble_device_base::ConnectionType::V3_WITHOUT_CACHE);
        this->log_connection_info_(connection, "v3 without cache");
      }
      connection->initiate_connection(static_cast<uint8_t>(msg.address_type));
      this->send_connections_free();
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection == nullptr) {
        this->answer_device_disconnected_(msg.address);
        this->send_connections_free();
        return;
      }
      if (connection->state() != ClientState::IDLE) {
        connection->disconnect();
      } else {
        connection->set_address(0);
        this->answer_device_disconnected_(msg.address);
        this->send_connections_free();
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR: {
      // The connection wrapper exposes the pairing surface; success is
      // reported when the platform's pairing completion arrives.
      auto *connection = this->get_connection_(msg.address, false);
      if (connection != nullptr) {
        if (!connection->is_paired()) {
          auto err = connection->pair();
          if (err != CONN_OK) {
            this->send_device_pairing(msg.address, false, err);
          }
        } else {
          this->send_device_pairing(msg.address, true);
        }
      } else {
        // Answer instead of leaving the client to time out.
        this->send_device_pairing(msg.address, false, GATT_NOT_CONNECTED);
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR: {
      conn_err_t ret = bluetooth_connection::unpair_device(msg.address);
      if (ret == CONN_OK) {
        // The bond is gone; a live connection must not short-circuit the
        // next PAIR as already paired.
        auto *connection = this->get_connection_(msg.address, false);
        if (connection != nullptr) {
          connection->set_unpaired();
        }
      }
      this->send_device_unpairing(msg.address, ret == CONN_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE: {
      conn_err_t ret = bluetooth_connection::clear_gatt_cache(msg.address);
      this->send_device_clear_cache(msg.address, ret == CONN_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT: {
      ESP_LOGE(TAG, "V1 connections removed");
      this->answer_device_disconnected_(msg.address);
      break;
    }
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "characteristic");
    return;
  }

  auto err = connection->read_characteristic(msg.handle);
  if (err != CONN_OK) {
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "characteristic");
    return;
  }

  auto err = connection->write_characteristic(msg.handle, msg.data, msg.data_len, msg.response);
  if (err != CONN_OK) {
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "descriptor");
    return;
  }

  auto err = connection->read_descriptor(msg.handle);
  if (err != CONN_OK) {
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "descriptor");
    return;
  }

  auto err = connection->write_descriptor(msg.handle, msg.data, msg.data_len, true);
  if (err != CONN_OK) {
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr || !connection->connected()) {
    this->handle_gatt_not_connected_(msg.address, 0, "get", "services");
    return;
  }
  if (!connection->has_gatt_services()) {
    ESP_LOGW(TAG, "[%d] [%s] No GATT services found", connection->get_connection_index(), connection->address_str());
    // Through the retrying sender: a drop must not leave discovery hanging.
    // Re-entry does not depend on the cursor - this branch is gated on
    // has_gatt_services() alone, so no restore is needed.
    connection->send_services_done_();
    return;
  }
  if (connection->send_service_ > 0) {
    // A request mid-stream restarts from the top so the requester always
    // gets the full list. No duplicate risk: the client accumulates batches
    // per request, and a same-session re-request only happens after the
    // previous request timed out and discarded its partial list.
    ESP_LOGD(TAG, "[%d] [%s] GetServices mid-stream, restarting", connection->get_connection_index(),
             connection->address_str());
    connection->send_service_ = 0;
    return;
  }
  if (connection->send_service_ == SERVICES_DONE_PENDING) {
    // A new request supersedes an owed done: the client accumulates batches
    // per request, so its fresh, empty accumulator plus a bare done would
    // cache as an empty database. The table is freed; the client's timeout
    // arbitrates.
    ESP_LOGW(TAG, "[%d] [%s] GetServices superseded an undelivered done; client timeout will retry",
             connection->get_connection_index(), connection->address_str());
    connection->send_service_ = DONE_SENDING_SERVICES;
    return;
  }
  if (connection->send_service_ == INIT_SENDING_SERVICES)  // Start sending services if not started yet
    connection->send_service_ = 0;
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "notify", "characteristic");
    return;
  }

  auto err = connection->notify_characteristic(msg.handle, msg.enable);
  if (err != CONN_OK) {
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg) {
  if (this->api_connection_ == nullptr)
    return;
  // Not latched (esp32 parity): the request is idempotent, so a drop resolves
  // via the client timeout and a retry gives the same answer. Still reported.

  auto *connection = this->get_connection_(msg.address, false);
  api::BluetoothSetConnectionParamsResponse resp;
  resp.address = msg.address;

  if (connection == nullptr || !connection->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot set connection params, not connected",
             connection ? static_cast<int>(connection->get_connection_index()) : -1,
             connection ? connection->address_str() : "unknown");
    resp.error = GATT_NOT_CONNECTED;
    if (!this->api_connection_->send_message(resp)) {
      this->log_reply_dropped_("Connection-params", msg.address);
    }
    return;
  }

  // Protobuf fields are uint32_t to future-proof the API if BLE ever supports wider values;
  // clamp to uint16_t since the current BLE spec defines these as 16-bit.
  constexpr uint32_t max_val = std::numeric_limits<uint16_t>::max();
  resp.error = connection->update_connection_params(static_cast<uint16_t>(std::min(msg.min_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.max_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.latency, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.timeout, max_val)));
  if (!this->api_connection_->send_message(resp)) {
    this->log_reply_dropped_("Connection-params", msg.address);
  }
}

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

#ifdef USE_ESP32

void BluetoothProxy::bluetooth_scanner_set_mode(bool active) {
  // esp32 only: BLEHub is the concrete tracker here, so these calls reach
  // tracker-native methods beyond the neutral contract.
  if (this->hub_->get_scan_active() == active) {
    return;
  }
  ESP_LOGD(TAG, "Setting scanner mode to %s", active ? "active" : "passive");
  this->hub_->set_scan_active(active);
  this->hub_->stop_scan();
  this->hub_->set_scan_continuous(
      true);  // Set this to true to automatically start scanning again when it has cleaned up.
}

#else  // !USE_ESP32

void BluetoothProxy::bluetooth_scanner_set_mode(bool active) {
  if (this->hub_->scan_active() != active) {
    ESP_LOGD(TAG, "Setting scanner mode to %s", active ? "active" : "passive");
    if (!this->hub_->request_scan_mode(active)) {
      // Passive-only controller asked for active scanning; the state report
      // below carries the real, unchanged mode so the subscriber does not
      // assume the change happened.
      ESP_LOGW(TAG, "Scanner mode %s not supported by this tracker", active ? "active" : "passive");
    }
  }
#ifndef USE_BLE_SCANNER_STATE_CALLBACK
  if (this->api_connection_ != nullptr) {
    // Reports the mode change; the sender also refreshes last_scan_running_, so
    // a failed restart (scan_running_ dropped by the tracker) is not reported
    // again by loop() on the next tick. A push hub reports the restart's
    // transitions (mode rides along) instead.
    this->send_polled_scanner_state_();
  }
#endif
}

#endif  // USE_ESP32

void BluetoothProxy::loop() {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Stream pending service-discovery batches every iteration; the streamer
  // handles a vanished API connection itself.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    this->connections_[i]->process_pending_services();
  }
#endif

  // Run advertisement flush / scanner-state poll every 100ms
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_advertisement_flush_time_ < 100)
    return;
  this->last_advertisement_flush_time_ = now;

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  if (this->connections_free_pending_ && this->api_connection_ != nullptr) {
    // Resend a dropped slot-state update, paced by the 100 ms gate so the
    // retry does not hammer the congestion it exists to survive. Every build
    // sends this at subscribe time (api_connection.cpp), so the drain
    // compiles on every proxy build.
    this->connections_free_pending_ = false;
    this->send_connections_free(this->api_connection_);
  }
#endif

  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr) {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
    // The API subscriber is gone: tear down any connections it left behind
    // (disconnect() on an already-disconnecting slot is a no-op).
    for (uint8_t i = 0; i < this->connection_count_; i++) {
      auto *connection = this->connections_[i];
      if (connection->get_address() != 0) {
        connection->disconnect();
      }
    }
#endif
    return;
  }

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Paced retries of owed per-slot notifications; subscriber swaps clear
  // stale latches before this runs.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    this->connections_[i]->flush_owed_replies_();
  }
  // Address-keyed, not slot-keyed, so it gets its own loop; bounded by
  // connection_count_ like the latch and clear helpers. Not pre-cleared:
  // the sender clears on success and re-latches on refusal, keeping the
  // latch's leading-edge warn honest (same shape as the unpair drain).
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto &owed = this->pending_disconnections_[i];
    if (owed.empty())
      continue;
    this->send_device_disconnected_(owed.address(), owed.error());
  }

  // An owed unpair reply. Not pre-cleared: the sender clears on success and
  // re-latches on refusal, keeping its leading-edge warn guard honest.
  if (!this->pending_unpairing_.empty()) {
    conn_err_t error = this->pending_unpairing_.error();
    this->send_device_unpairing(this->pending_unpairing_.address(), error == CONN_OK, error);
  }
#endif

#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Resend a dropped scanner-state push (see scanner_state_pending_).
  if (this->scanner_state_pending_) {
    this->send_scanner_state_(this->hub_->get_scanner_state());
  }
#else
  // This hub doesn't push scanner-state transitions; poll and report on
  // change. A hub gaining push emits the define and drops this poll.
  if (this->hub_->scan_running() != this->last_scan_running_) {
    this->send_polled_scanner_state_();
  }
#endif

#ifdef USE_WIFI
  // Wi-Fi (or a coexistence build that can fall back to it): every other
  // non-empty 100 ms tick (~200 ms) gives partial batches time to fill
  // toward BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE, so the air gets fewer,
  // fuller frames. Full batches still ship immediately from the queueing
  // path, and the owed-reply drains above keep the 100 ms cadence.
  if (this->response_.advertisements_len != 0) {
    if (this->adv_flush_toggle_) {
      this->flush_pending_advertisements_();
    }
    this->adv_flush_toggle_ = !this->adv_flush_toggle_;
  } else {
    // Nothing pending (idle, or a full batch just shipped inline): arm so
    // the next batch ships on the next tick.
    this->adv_flush_toggle_ = true;
  }
#else
  // No Wi-Fi in the build (ethernet): no airtime worth trading latency for,
  // so partial batches flush every tick.
  this->flush_pending_advertisements_();
#endif
}

void BluetoothProxy::reset_owed_replies_() {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->connections_free_pending_ = false;
#endif
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Owed on unsubscribe; on subscribe the trailing send_scanner_state_()
  // re-drives it from the hub, so clearing it there is free.
  this->scanner_state_pending_ = false;
#else
  // Force a poll-arm mismatch: a frame refused at subscribe time could
  // otherwise match the stale detector and never be retried. Inert on
  // unsubscribe: loop() returns at the no-subscriber gate before the
  // detector runs, and a re-subscribe re-arms this anyway.
  this->last_scan_running_ = !this->hub_->scan_running();
#endif
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->pending_unpairing_.clear();
  this->pending_disconnections_.fill({});
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    // Neither a partial stream's tail nor an owed done belongs to the next
    // session; silence (the client's timeout) arbitrates.
    auto *connection = this->connections_[i];
    connection->park_service_stream_();
    connection->clear_owed_flags_();
  }
#endif
}

void BluetoothProxy::subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags) {
  if (api_connection != this->api_connection_) {
    if (this->api_connection_ != nullptr) {
      // A previous subscriber still holds the slot. This is almost always a
      // stale connection from a client that dropped without a clean disconnect
      // and has not yet hit the keepalive timeout; rejecting the new
      // subscriber would silently starve it of advertisements until it
      // reconnects, so the newest subscriber wins instead.
      char old_peername[socket::SOCKADDR_STR_LEN];
      char new_peername[socket::SOCKADDR_STR_LEN];
      ESP_LOGW(TAG, "Subscription from %s (%s) replaces %s (%s)", api_connection->get_name(),
               api_connection->get_peername_to(new_peername), this->api_connection_->get_name(),
               this->api_connection_->get_peername_to(old_peername));
    }
    // Stale retry latches belong to the previous subscriber's session; a
    // re-subscribe by the current one keeps what it is still owed.
    this->reset_owed_replies_();
  }
  this->api_connection_ = api_connection;
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // get_scanner_state() is part of the push-hub surface (see BLEHubContract).
  this->send_scanner_state_(this->hub_->get_scanner_state());
#else
  this->send_polled_scanner_state_();
#endif
}

void BluetoothProxy::unsubscribe_api_connection(api::APIConnection *api_connection) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGV(TAG, "API connection is not subscribed");
    return;
  }
  this->api_connection_ = nullptr;
  this->reset_owed_replies_();
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
void BluetoothProxy::send_connections_free() {
  if (this->api_connection_ != nullptr) {
    this->send_connections_free(this->api_connection_);
  }
}

void BluetoothProxy::send_connections_free(api::APIConnection *api_connection) {
  // Latch only for the current subscriber: loop() resends to api_connection_.
  if (!api_connection->send_message(this->connections_free_response_) && api_connection == this->api_connection_) {
    // V like the api layer's own buffer-full log: a D would ride the same
    // full connection.
    ESP_LOGV(TAG, "Connections-free update deferred, TCP buffer full");
    this->connections_free_pending_ = true;
  }
}

bool BluetoothProxy::send_device_connection(uint64_t address, bool connected, uint16_t mtu, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing owed
  api::BluetoothDeviceConnectionResponse call;
  call.address = address;
  call.connected = connected;
  call.mtu = mtu;
  call.error = error;
  return this->api_connection_->send_message(call);
}

bool BluetoothProxy::send_gatt_services_done(uint64_t address) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing is owed, only a refused frame reports false
  api::BluetoothGATTGetServicesDoneResponse call;
  call.address = address;
  return this->api_connection_->send_message(call);
}

bool BluetoothProxy::send_gatt_error(uint64_t address, uint16_t handle, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing is owed, only a refused frame reports false
  api::BluetoothGATTErrorResponse call;
  call.address = address;
  call.handle = handle;
  call.error = error;
  return this->api_connection_->send_message(call);
}

void BluetoothProxy::send_device_pairing(uint64_t address, bool paired, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDevicePairingResponse call;
  call.address = address;
  call.paired = paired;
  call.error = error;

  if (!this->api_connection_->send_message(call)) {
    // Not latched: a retried PAIR is answered from is_paired(), so the client
    // recovers on its own. Still worth saying it happened.
    this->log_reply_dropped_("Pairing", address);
  }
}

void BluetoothProxy::send_device_unpairing(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  // An owed success is the authoritative answer: a later attempt for the
  // same address fails only because the first already removed the bond.
  if (!this->pending_unpairing_.empty() && this->pending_unpairing_.matches(address) &&
      this->pending_unpairing_.error() == CONN_OK) {
    success = true;
    error = CONN_OK;
  }
  api::BluetoothDeviceUnpairingResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  if (this->api_connection_->send_message(call)) {
    // A later unpair landing for an address that still has one owed would
    // otherwise have the drain repeat it.
    if (this->pending_unpairing_.matches(address)) {
      this->pending_unpairing_.clear();
    }
    return;
  }
  if (this->pending_unpairing_.empty()) {
    this->log_reply_deferred_("Unpair", address);
  } else if (!this->pending_unpairing_.matches(address)) {
    this->log_reply_displaced_("Unpair", this->pending_unpairing_.address(), address);
  }
  this->pending_unpairing_.set(address, error);
}

// GATT arm only: the advertisement-only arm no longer dispatches CLEAR_CACHE,
// so its response encoder would be dead weight there.
void BluetoothProxy::send_device_clear_cache(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceClearCacheResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  if (!this->api_connection_->send_message(call)) {
    // Not latched: clear-cache is idempotent, so a retry gives the same answer.
    this->log_reply_dropped_("Clear-cache", address);
  }
}
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
