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
  ESPBTUUID();

  static ESPBTUUID from_uint16(uint16_t uuid);

  static ESPBTUUID from_uint32(uint32_t uuid);

  static ESPBTUUID from_raw(const uint8_t *data);
  static ESPBTUUID from_raw_reversed(const uint8_t *data);

  static ESPBTUUID from_raw(const char *data, size_t length);
  static ESPBTUUID from_raw(const char *data) { return from_raw(data, strlen(data)); }
  static ESPBTUUID from_raw(const std::string &data) { return from_raw(data.c_str(), data.length()); }
  static ESPBTUUID from_raw(std::initializer_list<uint8_t> data) {
    return from_raw(reinterpret_cast<const char *>(data.begin()), data.size());
  }

  static ESPBTUUID from_uuid(esp_bt_uuid_t uuid);

  ESPBTUUID as_128bit() const;

  bool contains(uint8_t data1, uint8_t data2) const;

  bool operator==(const ESPBTUUID &uuid) const;
  bool operator!=(const ESPBTUUID &uuid) const { return !(*this == uuid); }

  esp_bt_uuid_t get_uuid() const;

  std::string to_string() const;
  const char *to_str(std::span<char, UUID_STR_LEN> output) const;

 protected:
  esp_bt_uuid_t uuid_;
};

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_UUID
#endif  // USE_ESP32
