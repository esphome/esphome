// ble_device.cpp
//
// Platform-neutral implementation of the shared BLE advertisement types.
// Parses raw BLE advertisement data into ESPBTDevice.

#include "ble_device.h"

#include "ble_aes_ccm.h"

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::ble_device_base {

static const char *const TAG = "ble_device_base";

// Longest advertisement payload worth hex-dumping at VERY_VERBOSE
// (legacy advertising: 31-byte adv + 31-byte scan response).
static constexpr size_t BLE_ADV_MAX_LOG_BYTES = 62;

// ---------------------------------------------------------------------------
// ESPBTUUID
// ---------------------------------------------------------------------------

ESPBTUUID ESPBTUUID::from_uint16(uint16_t uuid) {
  ESPBTUUID ret;
  ret.type_ = Type::UUID16;
  ret.uuid_.uuid16 = uuid;
  return ret;
}

ESPBTUUID ESPBTUUID::from_uint32(uint32_t uuid) {
  ESPBTUUID ret;
  ret.type_ = Type::UUID32;
  ret.uuid_.uuid32 = uuid;
  return ret;
}

ESPBTUUID ESPBTUUID::from_raw(const uint8_t *data) {
  ESPBTUUID ret;
  ret.type_ = Type::UUID128;
  memcpy(ret.uuid_.uuid128, data, 16);
  return ret;
}

ESPBTUUID ESPBTUUID::from_raw_reversed(const uint8_t *data) {
  ESPBTUUID ret;
  ret.type_ = Type::UUID128;
  for (int i = 0; i < 16; i++)
    ret.uuid_.uuid128[i] = data[15 - i];
  return ret;
}

ESPBTUUID ESPBTUUID::from_raw(const char *data, size_t length) {
  // Same text-parsing semantics as the historical esp32_ble::ESPBTUUID::from_raw.
  ESPBTUUID ret;
  if (length == 4) {
    // 16-bit UUID as 4-character hex string
    auto parsed = parse_hex<uint16_t>(data, length);
    if (parsed.has_value()) {
      ret.type_ = Type::UUID16;
      ret.uuid_.uuid16 = parsed.value();
    }
  } else if (length == 8) {
    // 32-bit UUID as 8-character hex string
    auto parsed = parse_hex<uint32_t>(data, length);
    if (parsed.has_value()) {
      ret.type_ = Type::UUID32;
      ret.uuid_.uuid32 = parsed.value();
    }
  } else if (length == 16) {
    // 16 raw bytes (little-endian 128-bit UUID)
    ret.type_ = Type::UUID128;
    memcpy(ret.uuid_.uuid128, reinterpret_cast<const uint8_t *>(data), 16);
  } else if (length == 36) {
    // Dashed text form XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    ret.type_ = Type::UUID128;
    int n = 0;
    for (size_t i = 0; i < length; i += 2) {
      if (data[i] == '-')
        i++;
      uint8_t msb = data[i];
      uint8_t lsb = data[i + 1];
      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid128[15 - n++] = ((msb & 0x0F) << 4) | (lsb & 0x0F);
    }
  } else {
    ESP_LOGE(TAG, "ERROR: UUID value not 4, 8, 16 or 36 bytes - %s", data);
  }
  return ret;
}

#ifdef USE_ESP32
ESPBTUUID ESPBTUUID::from_uuid(esp_bt_uuid_t uuid) {
  if (uuid.len == 0)  // the unset sentinel get_uuid() emits
    return {};
  if (uuid.len == ESP_UUID_LEN_16)
    return ESPBTUUID::from_uint16(uuid.uuid.uuid16);
  if (uuid.len == ESP_UUID_LEN_32)
    return ESPBTUUID::from_uint32(uuid.uuid.uuid32);
  return ESPBTUUID::from_raw(uuid.uuid.uuid128);
}

esp_bt_uuid_t ESPBTUUID::get_uuid() const {
  esp_bt_uuid_t ret;
  switch (this->type_) {
    case Type::UNSET:
      ret.len = 0;
      memset(&ret.uuid, 0, sizeof(ret.uuid));
      break;
    case Type::UUID16:
      ret.len = ESP_UUID_LEN_16;
      ret.uuid.uuid16 = this->uuid_.uuid16;
      break;
    case Type::UUID32:
      ret.len = ESP_UUID_LEN_32;
      ret.uuid.uuid32 = this->uuid_.uuid32;
      break;
    default:
    case Type::UUID128:
      ret.len = ESP_UUID_LEN_128;
      memcpy(ret.uuid.uuid128, this->uuid_.uuid128, ESP_UUID_LEN_128);
      break;
  }
  return ret;
}

void ESPBTDevice::parse_scan_rst(const esp32_ble::BLEScanResult &scan_result) {
  this->scan_result_ = &scan_result;
  // BLEScanResult's bda is most-significant octet first; the neutral ingest
  // takes the BLE controller (LSB-first) order, so reverse — address_uint64()/
  // address_str_to() then produce exactly the historical esp32 values.
  uint8_t mac_lsb_first[MAC_ADDRESS_SIZE];
  for (uint8_t i = 0; i < 6; i++)
    mac_lsb_first[i] = scan_result.bda[5 - i];
  this->from_scan_result(mac_lsb_first, scan_result.rssi, scan_result.ble_addr_type, scan_result.ble_adv,
                         scan_result.adv_data_len + scan_result.scan_rsp_len);
}
#endif  // USE_ESP32

ESPBTUUID ESPBTUUID::as_128bit() const {
  // Widening an unset UUID stays unset; expanding it would produce a set 0x0000 base UUID.
  if (this->type_ == Type::UNSET || this->type_ == Type::UUID128)
    return *this;
  uint8_t data[16];
  this->to_128bit_(data);
  return ESPBTUUID::from_raw(data);
}

bool ESPBTUUID::contains(uint8_t data1, uint8_t data2) const {
  // Adjacent byte-pair search — identical semantics to esp32_ble::ESPBTUUID::contains.
  switch (this->type_) {
    case Type::UNSET:
      return false;
    case Type::UUID16:
      return (this->uuid_.uuid16 >> 8) == data2 && (this->uuid_.uuid16 & 0xFF) == data1;
    case Type::UUID32:
      for (uint8_t i = 0; i < 3; i++) {
        bool a = ((this->uuid_.uuid32 >> i * 8) & 0xFF) == data1;
        bool b = ((this->uuid_.uuid32 >> (i + 1) * 8) & 0xFF) == data2;
        if (a && b)
          return true;
      }
      return false;
    case Type::UUID128:
      for (uint8_t i = 0; i < 15; i++) {
        if (this->uuid_.uuid128[i] == data1 && this->uuid_.uuid128[i + 1] == data2)
          return true;
      }
      return false;
  }
  return false;
}

const char *ESPBTUUID::to_str(char *buf) const {
  // Identical output format to esp32_ble::ESPBTUUID::to_str.
  char *pos = buf;
  switch (this->type_) {
    case Type::UNSET:
      memcpy(buf, "None", 5);
      return buf;
    case Type::UUID16:
      *pos++ = '0';
      *pos++ = 'x';
      *pos++ = format_hex_pretty_char(this->uuid_.uuid16 >> 12);
      *pos++ = format_hex_pretty_char((this->uuid_.uuid16 >> 8) & 0x0F);
      *pos++ = format_hex_pretty_char((this->uuid_.uuid16 >> 4) & 0x0F);
      *pos++ = format_hex_pretty_char(this->uuid_.uuid16 & 0x0F);
      *pos = 0;  // NUL-terminate
      return buf;
    case Type::UUID32:
      *pos++ = '0';
      *pos++ = 'x';
      for (int shift = 28; shift >= 0; shift -= 4)
        *pos++ = format_hex_pretty_char((this->uuid_.uuid32 >> shift) & 0x0F);
      *pos = 0;  // NUL-terminate
      return buf;
    default:
    case Type::UUID128:
      // Format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
      for (int8_t i = 15; i >= 0; i--) {
        uint8_t byte = this->uuid_.uuid128[i];
        *pos++ = format_hex_pretty_char(byte >> 4);
        *pos++ = format_hex_pretty_char(byte & 0x0F);
        if (i == 12 || i == 10 || i == 8 || i == 6)
          *pos++ = '-';
      }
      *pos = 0;  // NUL-terminate
      return buf;
  }
}

void ESPBTUUID::to_128bit_(uint8_t out[16]) const {
  // Bluetooth Base UUID 00000000-0000-1000-8000-00805F9B34FB (LSB-first), with the 16/32-bit
  // value placed at bytes 12..; identical expansion to esp32_ble::ESPBTUUID::as_128bit().
  // Callers screen out UNSET first (operator==, as_128bit); it would expand like 0x0000.
  static const uint8_t BASE[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                                   0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  if (this->type_ == Type::UUID128) {
    memcpy(out, this->uuid_.uuid128, 16);
    return;
  }
  memcpy(out, BASE, 16);
  const uint32_t value = (this->type_ == Type::UUID32) ? this->uuid_.uuid32 : this->uuid_.uuid16;
  const size_t len = (this->type_ == Type::UUID32) ? 4 : 2;
  for (size_t i = 0; i < len; i++)
    out[12 + i] = (value >> (i * 8)) & 0xFF;
}

bool ESPBTUUID::operator==(const ESPBTUUID &other) const {
  if (this->type_ == other.type_) {
    switch (this->type_) {
      case Type::UNSET:
        return true;
      case Type::UUID16:
        return this->uuid_.uuid16 == other.uuid_.uuid16;
      case Type::UUID32:
        return this->uuid_.uuid32 == other.uuid_.uuid32;
      case Type::UUID128:
        return memcmp(this->uuid_.uuid128, other.uuid_.uuid128, 16) == 0;
    }
    return false;
  }
  // Unset never equals a set UUID; 0x0000 is a valid value, distinct from "not configured".
  if (this->type_ == Type::UNSET || other.type_ == Type::UNSET)
    return false;
  // Different widths: expand both to the 128-bit Bluetooth Base UUID form and compare, so a
  // configured 16/32-bit UUID matches the equivalent 128-bit advertisement (esp32 parity).
  uint8_t a[16];
  uint8_t b[16];
  this->to_128bit_(a);
  other.to_128bit_(b);
  return memcmp(a, b, 16) == 0;
}

// ---------------------------------------------------------------------------
// ESPBLEiBeacon
// ---------------------------------------------------------------------------

ESPBLEiBeacon::ESPBLEiBeacon(const uint8_t *data) { memcpy(&this->beacon_data_, data, sizeof(this->beacon_data_)); }

optional<ESPBLEiBeacon> ESPBLEiBeacon::from_manufacturer_data(const ServiceData &data, bool *prefix_rejected) {
  // iBeacon manufacturer specific data (after company-ID bytes have been stripped):
  //   [0x02][0x15][16-byte UUID][2-byte major][2-byte minor][1-byte power] = exactly 23 bytes
  if (!data.uuid.contains(0x4C, 0x00))  // Apple company ID 0x004C
    return {};
  if (data.data.size() != 23)
    return {};
  // Require the iBeacon sub-type/length prefix — stricter than the legacy
  // esp32 parser, which accepted any 23-byte Apple payload and surfaced
  // non-iBeacon frames as garbage beacons.
  if (data.data[0] != 0x02 || data.data[1] != 0x15) {
    if (prefix_rejected != nullptr)
      *prefix_rejected = true;
    return {};
  }
  return ESPBLEiBeacon(data.data.data());
}

// ---------------------------------------------------------------------------
// ESPBTDevice
// ---------------------------------------------------------------------------

optional<ESPBLEiBeacon> ESPBTDevice::get_ibeacon() const {
  bool prefix_rejected = false;
  uint8_t rejected_sub_type = 0;
  uint8_t rejected_len = 0;
  for (const auto &it : this->manufacturer_datas_) {
    bool rejected = false;
    auto res = ESPBLEiBeacon::from_manufacturer_data(it, &rejected);
    if (res.has_value())
      return res;
    if (rejected && !prefix_rejected) {
      prefix_rejected = true;
      rejected_sub_type = it.data[0];
      rejected_len = it.data[1];
    }
  }
  if (prefix_rejected) {
    // Only when no beacon was found at all: these frames were accepted before
    // the prefix check, so their disappearance must be observable at the
    // default log level. Throttled so a chatty non-iBeacon Apple advertiser
    // cannot flood the log; a different address may bypass the shared window
    // so that advertiser cannot mask the device that actually regressed — but
    // with a 1 s floor, or two alternating advertisers log every frame.
    static uint32_t last_log = 0;
    static uint64_t last_addr = 0;
    const uint32_t now = millis();
    const uint64_t addr = this->address_uint64();
    const uint32_t since = now - last_log;
    if (last_log == 0 || since > 60000 || (addr != last_addr && since > 1000)) {
      last_log = now;
      last_addr = addr;
      char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      ESP_LOGD(TAG, "%s: 23-byte Apple frame without iBeacon prefix ignored (sub-type 0x%02X len 0x%02X)",
               this->address_str_to(addr_buf), rejected_sub_type, rejected_len);
    }
  }
  return {};
}

const char *ESPBTDevice::address_type_str() const {
  switch (this->address_type_) {
    case BLE_ADDR_TYPE_PUBLIC:
      return "PUBLIC";
    case BLE_ADDR_TYPE_RANDOM:
      return "RANDOM";
    case BLE_ADDR_TYPE_RPA_PUBLIC:
      return "RPA_PUBLIC";
    case BLE_ADDR_TYPE_RPA_RANDOM:
      return "RPA_RANDOM";
    default:
      return "UNKNOWN";
  }
}

void ESPBTDevice::from_scan_result(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data,
                                   uint16_t data_len) {
  // Ingest is BLE controller order (LSB-first); store in printable (MSB-first)
  // order so the raw address() accessor matches the historical esp32 layout.
  for (uint8_t i = 0; i < 6; i++)
    this->address_[i] = mac[5 - i];
  this->address_type_ = addr_type;
  this->rssi_ = rssi;
  this->name_len_ = 0;
  this->name_[0] = '\0';
  this->service_uuids_.clear();
  this->manufacturer_datas_.clear();
  this->service_datas_.clear();
  this->tx_powers_.clear();
  this->appearance_.reset();
  this->ad_flag_.reset();
  this->parse_adv_(data, data_len);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGVV(TAG,
            "Parse Result:\n"
            "  Address: %s (%s)\n"
            "  RSSI: %d\n"
            "  Name: '%s'",
            this->address_str_to(addr_buf), this->address_type_str(), this->rssi_, this->name_);
  for (auto &it : this->tx_powers_) {
    ESP_LOGVV(TAG, "  TX Power: %d", it);
  }
  if (this->appearance_.has_value()) {
    ESP_LOGVV(TAG, "  Appearance: %u", *this->appearance_);
  }
  if (this->ad_flag_.has_value()) {
    ESP_LOGVV(TAG, "  Ad Flag: %u", *this->ad_flag_);
  }
  char uuid_buf[UUID_STR_LEN];
  for (auto &uuid : this->service_uuids_) {
    ESP_LOGVV(TAG, "  Service UUID: %s", uuid.to_str(uuid_buf));
  }
  char hex_buf[format_hex_pretty_size(BLE_ADV_MAX_LOG_BYTES)];
  for (auto &mfg_data : this->manufacturer_datas_) {
    auto ibeacon = ESPBLEiBeacon::from_manufacturer_data(mfg_data);
    if (ibeacon.has_value()) {
      ESP_LOGVV(TAG,
                "  Manufacturer iBeacon:\n"
                "    UUID: %s\n"
                "    Major: %u\n"
                "    Minor: %u\n"
                "    TXPower: %d",
                ibeacon.value().get_uuid().to_str(uuid_buf), ibeacon.value().get_major(), ibeacon.value().get_minor(),
                ibeacon.value().get_signal_power());
    } else {
      ESP_LOGVV(TAG, "  Manufacturer ID: %s, data: %s", mfg_data.uuid.to_str(uuid_buf),
                format_hex_pretty_to(hex_buf, mfg_data.data.data(), mfg_data.data.size()));
    }
  }
  for (auto &svc_data : this->service_datas_) {
    ESP_LOGVV(TAG,
              "  Service data:\n"
              "    UUID: %s\n"
              "    Data: %s",
              svc_data.uuid.to_str(uuid_buf),
              format_hex_pretty_to(hex_buf, svc_data.data.data(), svc_data.data.size()));
  }
  ESP_LOGVV(TAG, "  Adv data: %s", format_hex_pretty_to(hex_buf, data, data_len));
#endif  // ESPHOME_LOG_HAS_VERY_VERBOSE
}

// Remove before 2027.2.0
std::string ESPBTDevice::address_str() const {
  char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  return std::string(this->address_str_to(buf));
}

const char *ESPBTDevice::address_str_to(char *buf) const {
  // address_ is stored in printable (MSB-first) order.
  format_mac_addr_upper(this->address_, buf);
  return buf;
}

uint64_t ESPBTDevice::address_uint64() const {
  // address_ is MSB-first; byte 0 of the result is the LSB (esp32 semantics).
  uint64_t addr = 0;
  for (int i = 0; i < 6; i++)
    addr |= static_cast<uint64_t>(this->address_[i]) << ((5 - i) * 8);
  return addr;
}

bool ESPBTDevice::resolve_irk(const uint8_t *irk) const {
#ifdef USE_BLE_DEVICE_IRK
  // Bluetooth Core 5.x "ah" function: hash = e(IRK, padding | prand)[low 24 bits].
  // The resolvable private address is prand (top 3 bytes) | hash (bottom 3 bytes).
  // Uses the portable software AES-128 shared with the CCM decryptor, so IRK
  // matching behaves identically on every platform (volume is one block per
  // advertisement from a matching RPA device — software AES is not a cost).
  uint8_t ecb_plaintext[16] = {0};
  uint8_t ecb_ciphertext[16];
  const uint64_t addr64 = this->address_uint64();
  ecb_plaintext[13] = (addr64 >> 40) & 0xff;
  ecb_plaintext[14] = (addr64 >> 32) & 0xff;
  ecb_plaintext[15] = (addr64 >> 24) & 0xff;
  aes128_encrypt_block(irk, ecb_plaintext, ecb_ciphertext);
  return ecb_ciphertext[15] == (addr64 & 0xff) && ecb_ciphertext[14] == ((addr64 >> 8) & 0xff) &&
         ecb_ciphertext[13] == ((addr64 >> 16) & 0xff);
#else
  // No sensor configured an irk: in this build; the AES core is compiled out.
  (void) irk;
  return false;
#endif
}

void ESPBTDevice::parse_adv_(const uint8_t *payload, uint16_t len) {
  // BLE AD structure TLV: [length][type][value...]
  // length includes the type byte.
  uint16_t offset = 0;
  while (offset < len) {
    uint8_t ad_len = payload[offset++];
    if (ad_len == 0)
      continue;  // possible zero-padded advertisement data (esp32_ble_tracker skips these too)
    if (offset + ad_len > len)
      break;
    uint8_t ad_type = payload[offset];
    const uint8_t *ad_data = &payload[offset + 1];
    uint8_t ad_data_len = ad_len - 1;
    offset += ad_len;

    switch (ad_type) {
      case 0x01:  // Flags
        if (ad_data_len >= 1)
          this->ad_flag_ = ad_data[0];
        break;

      case 0x08:  // Shortened Local Name
      case 0x09:  // Complete Local Name
        // Keep the longest name seen — a merged adv + scan-response frame may carry both the
        // shortened and the complete name, and the shortened form must never replace the
        // complete one (same rule as esp32_ble_tracker's parse_adv_).
        if (ad_data_len > this->name_len_) {
          uint8_t name_len = ad_data_len > MAX_ADV_NAME_LEN ? MAX_ADV_NAME_LEN : static_cast<uint8_t>(ad_data_len);
          memcpy(this->name_, ad_data, name_len);
          this->name_[name_len] = '\0';
          this->name_len_ = name_len;
        }
        break;

      case 0x0A:  // TX Power Level
        if (ad_data_len >= 1)
          this->tx_powers_.push_back(static_cast<int8_t>(ad_data[0]));
        break;

      case 0x19:  // Appearance
        if (ad_data_len >= 2)
          this->appearance_ = static_cast<uint16_t>(ad_data[0]) | (static_cast<uint16_t>(ad_data[1]) << 8);
        break;

      case 0x02:  // Incomplete List of 16-bit Service UUIDs
      case 0x03:  // Complete List of 16-bit Service UUIDs
        for (uint8_t i = 0; (i + 1) < ad_data_len; i += 2) {
          uint16_t uuid = (static_cast<uint16_t>(ad_data[i + 1]) << 8) | ad_data[i];
          this->service_uuids_.push_back(ESPBTUUID::from_uint16(uuid));
        }
        break;

      case 0x04:  // Incomplete List of 32-bit Service UUIDs
      case 0x05:  // Complete List of 32-bit Service UUIDs
        for (uint8_t i = 0; (i + 3) < ad_data_len; i += 4) {
          uint32_t uuid = (static_cast<uint32_t>(ad_data[i + 3]) << 24) |
                          (static_cast<uint32_t>(ad_data[i + 2]) << 16) | (static_cast<uint32_t>(ad_data[i + 1]) << 8) |
                          ad_data[i];
          this->service_uuids_.push_back(ESPBTUUID::from_uint32(uuid));
        }
        break;

      case 0x06:  // Incomplete List of 128-bit Service UUIDs
      case 0x07:  // Complete List of 128-bit Service UUIDs
        for (uint8_t i = 0; (i + 15) < ad_data_len; i += 16)
          this->service_uuids_.push_back(ESPBTUUID::from_raw(&ad_data[i]));
        break;

      case 0xFF:  // Manufacturer Specific Data
        if (ad_data_len >= 2) {
          uint16_t company_id = (static_cast<uint16_t>(ad_data[1]) << 8) | ad_data[0];
          ServiceData sd;
          sd.uuid = ESPBTUUID::from_uint16(company_id);
          sd.data.assign(ad_data + 2, ad_data + ad_data_len);
          this->manufacturer_datas_.push_back(std::move(sd));
        }
        break;

      case 0x16:  // Service Data — 16-bit UUID
        if (ad_data_len >= 2) {
          uint16_t uuid = (static_cast<uint16_t>(ad_data[1]) << 8) | ad_data[0];
          ServiceData sd;
          sd.uuid = ESPBTUUID::from_uint16(uuid);
          sd.data.assign(ad_data + 2, ad_data + ad_data_len);
          this->service_datas_.push_back(std::move(sd));
        }
        break;

      case 0x20:  // Service Data — 32-bit UUID
        if (ad_data_len >= 4) {
          uint32_t uuid = (static_cast<uint32_t>(ad_data[3]) << 24) | (static_cast<uint32_t>(ad_data[2]) << 16) |
                          (static_cast<uint32_t>(ad_data[1]) << 8) | ad_data[0];
          ServiceData sd;
          sd.uuid = ESPBTUUID::from_uint32(uuid);
          sd.data.assign(ad_data + 4, ad_data + ad_data_len);
          this->service_datas_.push_back(std::move(sd));
        }
        break;

      case 0x21:  // Service Data — 128-bit UUID
        if (ad_data_len >= 16) {
          ServiceData sd;
          sd.uuid = ESPBTUUID::from_raw(ad_data);
          sd.data.assign(ad_data + 16, ad_data + ad_data_len);
          this->service_datas_.push_back(std::move(sd));
        }
        break;

      default:
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// DiscoveredDeviceLog
// ---------------------------------------------------------------------------

void DiscoveredDeviceLog::log_device(const char *tag, const ESPBTDevice &device) {
#ifdef ESPHOME_LOG_HAS_DEBUG
  // Everything here feeds ESP_LOGD: below DEBUG the whole body (including the
  // dedup vector growth) would be pure overhead, so compile it out entirely.
  const uint64_t address = device.address_uint64();
  for (auto &disc : this->already_discovered_) {
    if (disc == address)
      return;
  }
  this->already_discovered_.push_back(address);

  char addr_buf[ESPBTDevice::MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGD(tag,
           "Found device %s RSSI=%d\n"
           "  Address Type: %s",
           device.address_str_to(addr_buf), device.get_rssi(), device.address_type_str());
  if (!device.get_name().empty()) {
    ESP_LOGD(tag, "  Name: '%s'", device.get_name().c_str());
  }
  for (auto &tx_power : device.get_tx_powers()) {
    ESP_LOGD(tag, "  TX Power: %d", tx_power);
  }
#endif  // ESPHOME_LOG_HAS_DEBUG
}

}  // namespace esphome::ble_device_base
