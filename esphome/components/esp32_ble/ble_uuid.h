#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_UUID

#include <initializer_list>
#include <span>
#include <string>
#include <esp_bt_defs.h>

namespace esphome::esp32_ble {

/// Buffer size for UUID string: "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\0"
static constexpr size_t UUID_STR_LEN = 37;

class ESPBTUUID {
 public:
  constexpr ESPBTUUID() : uuid_{} {}

  static constexpr ESPBTUUID from_uint16(uint16_t uuid) {
    ESPBTUUID ret;
    ret.uuid_.len = ESP_UUID_LEN_16;
    ret.uuid_.uuid.uuid16 = uuid;
    return ret;
  }

  static constexpr ESPBTUUID from_uint32(uint32_t uuid) {
    ESPBTUUID ret;
    ret.uuid_.len = ESP_UUID_LEN_32;
    ret.uuid_.uuid.uuid32 = uuid;
    return ret;
  }

  static constexpr ESPBTUUID from_raw(const uint8_t *data) {
    ESPBTUUID ret;
    ret.uuid_.len = ESP_UUID_LEN_128;
    for (uint8_t i = 0; i < ESP_UUID_LEN_128; i++)
      ret.uuid_.uuid.uuid128[i] = data[i];
    return ret;
  }

  static constexpr ESPBTUUID from_raw_reversed(const uint8_t *data) {
    ESPBTUUID ret;
    ret.uuid_.len = ESP_UUID_LEN_128;
    for (uint8_t i = 0; i < ESP_UUID_LEN_128; i++)
      ret.uuid_.uuid.uuid128[ESP_UUID_LEN_128 - 1 - i] = data[i];
    return ret;
  }

  static constexpr ESPBTUUID from_raw(const char *data, size_t length) {
    ESPBTUUID ret;
    if (length == 4) {
      // 16-bit UUID as 4-character hex string
      ret.uuid_.len = ESP_UUID_LEN_16;
      ret.uuid_.uuid.uuid16 = parse_hex_16_(data);
    } else if (length == 8) {
      // 32-bit UUID as 8-character hex string
      ret.uuid_.len = ESP_UUID_LEN_32;
      ret.uuid_.uuid.uuid32 = parse_hex_32_(data);
    } else if (length == 16) {
      // 16 raw bytes
      ret.uuid_.len = ESP_UUID_LEN_128;
      for (uint8_t i = 0; i < ESP_UUID_LEN_128; i++)
        ret.uuid_.uuid.uuid128[i] = static_cast<uint8_t>(data[i]);
    } else if (length == 36) {
      // UUID format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
      ret.uuid_.len = ESP_UUID_LEN_128;
      int n = 0;
      for (size_t i = 0; i < 36; i += 2) {
        if (data[i] == '-')
          i++;
        ret.uuid_.uuid.uuid128[15 - n++] = (parse_hex_char(data[i]) << 4) | parse_hex_char(data[i + 1]);
      }
    }
    // Invalid length returns empty UUID (len=0)
    return ret;
  }

  static constexpr ESPBTUUID from_raw(const char *data) { return from_raw(data, c_strlen_(data)); }
  static ESPBTUUID from_raw(const std::string &data) { return from_raw(data.c_str(), data.length()); }
  static constexpr ESPBTUUID from_raw(std::initializer_list<char> data) { return from_raw(data.begin(), data.size()); }

  static ESPBTUUID from_uuid(esp_bt_uuid_t uuid);

  ESPBTUUID as_128bit() const;

  bool contains(uint8_t data1, uint8_t data2) const;

  bool operator==(const ESPBTUUID &uuid) const;
  bool operator!=(const ESPBTUUID &uuid) const { return !(*this == uuid); }

  esp_bt_uuid_t get_uuid() const;

  std::string to_string() const;
  void to_str(std::span<char, UUID_STR_LEN> output) const;

 protected:
  esp_bt_uuid_t uuid_;

 private:
  static constexpr uint16_t parse_hex_16_(const char *s) {
    return (parse_hex_char(s[0]) << 12) | (parse_hex_char(s[1]) << 8) | (parse_hex_char(s[2]) << 4) |
           parse_hex_char(s[3]);
  }

  static constexpr uint32_t parse_hex_32_(const char *s) {
    return (static_cast<uint32_t>(parse_hex_16_(s)) << 16) | parse_hex_16_(s + 4);
  }

  static constexpr size_t c_strlen_(const char *s) {
    size_t len = 0;
    while (s[len] != '\0')
      len++;
    return len;
  }
};

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_UUID
#endif  // USE_ESP32
