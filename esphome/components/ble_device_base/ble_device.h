// ble_device.h
//
// Platform-neutral BLE advertisement types — the generic base every BLE consumer
// (sensor components, bluetooth_proxy, automation triggers) builds against:
//   ESPBTUUID / ServiceData / ESPBLEiBeacon / ESPBTDevice / ESPBTDeviceListener
//
// These types are owned here on EVERY platform, with no chip-SDK types in their
// public surface. Platform trackers produce them:
//   - esp32_ble_tracker adapts ESP-IDF scan results into ESPBTDevice and
//     re-exports these names (esp32 only) for backward compatibility;
//   - the LibreTiny trackers (bk72xx / ln882h) feed from_scan_result() directly.

#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#if defined(__cpp_lib_span)
#include <span>
#endif

#ifdef USE_ESP32
// Historical esp32_ble API surface (below, under the same define) uses the
// ESP-IDF UUID/address/scan-result types directly; never referenced off-esp32.
#include "esphome/components/esp32_ble/ble_scan_result.h"
#include <esp_bt_defs.h>
#endif

namespace esphome::ble_device_base {

using adv_data_t = std::vector<uint8_t>;

// Bluetooth Core address types (spec values; matches ESP-IDF's esp_ble_addr_type_t).
static constexpr uint8_t BLE_ADDR_TYPE_PUBLIC = 0;
static constexpr uint8_t BLE_ADDR_TYPE_RANDOM = 1;
static constexpr uint8_t BLE_ADDR_TYPE_RPA_PUBLIC = 2;
static constexpr uint8_t BLE_ADDR_TYPE_RPA_RANDOM = 3;

/// Buffer size for UUID string: "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\0"
static constexpr size_t UUID_STR_LEN = 37;

// ---------------------------------------------------------------------------
// ESPBTUUID — 16/32/128-bit Bluetooth UUID value type.
// API-compatible with the historical esp32_ble::ESPBTUUID; the esp_bt_uuid_t
// conversions live in esp32_ble (esp32-only adapters), not here.
// ---------------------------------------------------------------------------

class ESPBTUUID {
 public:
  ESPBTUUID() = default;

  static ESPBTUUID from_uint16(uint16_t uuid);
  static ESPBTUUID from_uint32(uint32_t uuid);
  /// Construct from raw 16-byte little-endian UUID.
  static ESPBTUUID from_raw(const uint8_t *data);
  /// Construct from raw 16-byte big-endian UUID (reversed on store).
  static ESPBTUUID from_raw_reversed(const uint8_t *data);
  /// Parse from text: 4 hex chars (16-bit), 8 hex chars (32-bit), 16 raw bytes,
  /// or the 36-char dashed UUID form. Same semantics as esp32_ble historically.
  static ESPBTUUID from_raw(const char *data, size_t length);
  static ESPBTUUID from_raw(const char *data) { return from_raw(data, strlen(data)); }
  static ESPBTUUID from_raw(const std::string &data) { return from_raw(data.c_str(), data.length()); }
  static ESPBTUUID from_raw(std::initializer_list<uint8_t> data) {
    return from_raw(reinterpret_cast<const char *>(data.begin()), data.size());
  }

#ifdef USE_ESP32
  /// Source compatibility with the historical esp32_ble API (esp32 builds only).
  static ESPBTUUID from_uuid(esp_bt_uuid_t uuid);
  esp_bt_uuid_t get_uuid() const;
#endif

  /// Expand to the 128-bit Bluetooth Base UUID form.
  ESPBTUUID as_128bit() const;

  /// True if the UUID value contains the adjacent byte pair (data1, data2).
  bool contains(uint8_t data1, uint8_t data2) const;

  bool operator==(const ESPBTUUID &other) const;
  bool operator!=(const ESPBTUUID &other) const { return !(*this == other); }

  /// Write "0xABCD" / "0xABCDEF01" / the dashed 128-bit form, or "None" for an
  /// unset UUID, into buf (>= UUID_STR_LEN bytes) and return buf.
  const char *to_str(char *buf) const;
#if defined(__cpp_lib_span)
  const char *to_str(std::span<char, UUID_STR_LEN> output) const { return this->to_str(output.data()); }
#endif
  // UNSET is the default-constructed state; get_uuid() reports it as len 0 (the historical sentinel).
  enum class Type : uint8_t { UNSET, UUID16, UUID32, UUID128 };
  Type type() const { return this->type_; }
  /// True if a UUID has been configured (not default-constructed).
  bool is_set() const { return this->type_ != Type::UNSET; }
  uint16_t uuid16() const { return this->uuid_.uuid16; }
  uint32_t uuid32() const { return this->uuid_.uuid32; }
  const uint8_t *uuid128() const { return this->uuid_.uuid128; }

 protected:
  // Expand to the 128-bit Bluetooth Base UUID byte form (out is 16 bytes, little-endian).
  void to_128bit_(uint8_t out[16]) const;

  Type type_{Type::UNSET};
  union {
    uint16_t uuid16;
    uint32_t uuid32;
    uint8_t uuid128[16];
  } uuid_{};
};

// ---------------------------------------------------------------------------
// ServiceData — UUID-tagged advertisement payload (0x16 / 0xFF AD types)
// ---------------------------------------------------------------------------

struct ServiceData {
  ESPBTUUID uuid;
  adv_data_t data;
};

// ---------------------------------------------------------------------------
// ESPBLEiBeacon
// ---------------------------------------------------------------------------

class ESPBLEiBeacon {
 public:
  ESPBLEiBeacon() { memset(&this->beacon_data_, 0, sizeof(this->beacon_data_)); }
  explicit ESPBLEiBeacon(const uint8_t *data);
  /// prefix_rejected: caller must initialise to false; set to true ONLY when a
  /// 23-byte Apple frame was refused for lacking the 0x02/0x15 iBeacon prefix —
  /// the case the legacy esp32 parser accepted. Never written on accept or on
  /// the non-Apple/wrong-size rejects. The caller with the device address does
  /// the logging (see ESPBTDevice::get_ibeacon()).
  static optional<ESPBLEiBeacon> from_manufacturer_data(const ServiceData &data, bool *prefix_rejected = nullptr);

  uint16_t get_major() const { return byteswap(this->beacon_data_.major); }
  uint16_t get_minor() const { return byteswap(this->beacon_data_.minor); }
  int8_t get_signal_power() const { return this->beacon_data_.signal_power; }
  ESPBTUUID get_uuid() const { return ESPBTUUID::from_raw_reversed(this->beacon_data_.proximity_uuid); }

 protected:
  struct PACKED BeaconData {
    uint8_t sub_type;
    uint8_t length;
    uint8_t proximity_uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t signal_power;
  } beacon_data_;
};

/// Pack a controller-order (LSB-first) MAC into the uint64 the API speaks.
/// Trackers with LSB-native SDKs call this at the emit site before filling
/// RawAdvertisement::address; ESPBTDevice::address_uint64() is the equivalent
/// for an already parsed device, whose address is stored MSB-first.
inline uint64_t mac_lsb_first_to_uint64(const uint8_t *mac) {
  uint64_t addr = 0;
  for (int i = 0; i < 6; i++)
    addr |= static_cast<uint64_t>(mac[i]) << (i * 8);
  return addr;
}

/// Unpack a uint64 BLE address into printable (MSB-first) byte order —
/// the order bd_addr_t / esp_bd_addr_t style APIs expect.
inline void uint64_to_mac_msb_first(uint64_t address, uint8_t out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = (address >> ((5 - i) * 8)) & 0xFF;
}

// ---------------------------------------------------------------------------
// ESPBTDevice — parsed BLE advertisement
// ---------------------------------------------------------------------------

class ESPBTDevice {
 public:
  /// Populate from a raw scan result delivered by a BLE tracker backend.
  /// mac is least-significant octet first (BLE controller convention).
  void from_scan_result(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len);

  // Alias the core constant so the two cannot drift apart.
  static constexpr size_t MAC_ADDRESS_PRETTY_BUFFER_SIZE = esphome::MAC_ADDRESS_PRETTY_BUFFER_SIZE;

  /// Return MAC as "XX:XX:XX:XX:XX:XX" string.
  ESPDEPRECATED("Use address_str_to() instead. Removed in 2027.2.0.", "2026.8.0")
  std::string address_str() const;
  /// Writes "XX:XX:XX:XX:XX:XX\0" into buf (>= MAC_ADDRESS_PRETTY_BUFFER_SIZE bytes), returns buf.
  const char *address_str_to(char *buf) const;
#if defined(__cpp_lib_span)
  const char *address_str_to(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) const {
    return this->address_str_to(buf.data());
  }
#endif
  /// Return MAC as packed uint64 (byte 0 in LSB — matches esp32's address_uint64).
  uint64_t address_uint64() const;
  /// Raw MAC bytes in printable (MSB-first) order — matches the historical
  /// esp32 layout (ESP-IDF bda order).
  const uint8_t *address() const { return address_; }
#ifdef USE_ESP32
  // Historical esp32 signature: consumers assign the result to esp_ble_addr_type_t.
  esp_ble_addr_type_t get_address_type() const { return static_cast<esp_ble_addr_type_t>(this->address_type_); }
  /// Historical esp32 ingest (esp32 builds only): parse an ESP-IDF scan result.
  /// Prefer ESPBTDevice::from_scan_result(); deprecation is a follow-up pending
  /// consumer feedback on the raw scan-result fields.
  void parse_scan_rst(const esp32_ble::BLEScanResult &scan_result);
  // Exposed through a function for use in lambdas
  const esp32_ble::BLEScanResult &get_scan_result() const { return *scan_result_; }
#else
  uint8_t get_address_type() const { return this->address_type_; }
#endif
  /// Human-readable address type ("PUBLIC", "RANDOM", "RPA_PUBLIC", "RPA_RANDOM" or
  /// "UNKNOWN"), backed by the shared BLE_ADDR_TYPE_* constants above.
  const char *address_type_str() const;

  int get_rssi() const { return rssi_; }
  /// Advertised name as a view into the fixed buffer (always NUL-terminated,
  /// so c_str() is safe); converts implicitly to std::string where needed.
  StringRef get_name() const { return StringRef(this->name_, this->name_len_); }

  const std::vector<ESPBTUUID> &get_service_uuids() const { return service_uuids_; }
  const std::vector<ServiceData> &get_manufacturer_datas() const { return manufacturer_datas_; }
  const std::vector<ServiceData> &get_service_datas() const { return service_datas_; }
  const std::vector<int8_t> &get_tx_powers() const { return tx_powers_; }
  const optional<uint16_t> &get_appearance() const { return appearance_; }
  const optional<uint8_t> &get_ad_flag() const { return ad_flag_; }

  /// Resolve a Resolvable Private Address against a 16-byte IRK (Bluetooth "ah"
  /// function, AES-128). Uses the portable software AES shared with the CCM
  /// decryptor; compiled only when a sensor configures irk: (request_irk_support).
  bool resolve_irk(const uint8_t *irk) const;

  optional<ESPBLEiBeacon> get_ibeacon() const;

 protected:
  void parse_adv_(const uint8_t *payload, uint16_t len);

  // Max name bytes in a legacy advertisement AD element (31-byte PDU minus
  // the 2-byte element header); every in-tree tracker scans legacy PDUs only.
  static constexpr uint8_t MAX_ADV_NAME_LEN = 29;

  uint8_t address_[MAC_ADDRESS_SIZE]{0};
  uint8_t address_type_{0};
  int rssi_{0};
  // Fixed buffer instead of std::string: no per-advertisement heap churn on
  // the scan path, and no libstdc++ string/exception machinery in the image.
  char name_[MAX_ADV_NAME_LEN + 1]{};
  uint8_t name_len_{0};
  std::vector<ESPBTUUID> service_uuids_{};
  std::vector<ServiceData> manufacturer_datas_{};
  std::vector<ServiceData> service_datas_{};
#ifdef USE_ESP32
  const esp32_ble::BLEScanResult *scan_result_{nullptr};
#endif
  std::vector<int8_t> tx_powers_{};
  optional<uint16_t> appearance_{};
  optional<uint8_t> ad_flag_{};
};

// ---------------------------------------------------------------------------
// DiscoveredDeviceLog — shared per-scan-period "Found device" DEBUG logger
// ---------------------------------------------------------------------------

/// Per-scan-period "Found device" DEBUG logger, deduplicated by MAC address.
/// Shared by all tracker backends so the output format and dedup behaviour stay
/// identical by construction (single implementation instead of per-chip copies).
class DiscoveredDeviceLog {
 public:
  /// Log the device at DEBUG the first time its MAC is seen this scan period.
  void log_device(const char *tag, const ESPBTDevice &device);
  /// Reset the per-period dedup list (call when a scan period ends).
  void clear() { this->already_discovered_.clear(); }

 protected:
  std::vector<uint64_t> already_discovered_;
};

// ---------------------------------------------------------------------------
// ESPBTDeviceListener — base class for BLE consumers (sensors, proxy, triggers)
// ---------------------------------------------------------------------------

class ESPBTDeviceListener {
 public:
  virtual ~ESPBTDeviceListener() = default;
  /// Called at the end of each scan duration period.
  virtual void on_scan_end() {}
  virtual bool parse_device(const ESPBTDevice &device) = 0;
};

}  // namespace esphome::ble_device_base
