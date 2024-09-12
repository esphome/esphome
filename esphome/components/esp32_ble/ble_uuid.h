#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <string>

namespace esphome {
namespace esp32_ble {

class ESPBTUUID {
 public:
  ESPBTUUID();

  static ESPBTUUID from_uint16(uint16_t uuid);

  static ESPBTUUID from_uint32(uint32_t uuid);

  static ESPBTUUID from_raw(const uint8_t *data);

  static ESPBTUUID from_raw(const std::string &data);

  static ESPBTUUID from_raw(const char *uuid, size_t len);

  static ESPBTUUID from_uuid(esp_bt_uuid_t uuid);

  ESPBTUUID as_128bit() const;

  bool contains(uint8_t data1, uint8_t data2) const;

  bool operator==(const ESPBTUUID &uuid) const;
  bool operator!=(const ESPBTUUID &uuid) const { return !(*this == uuid); }

  esp_bt_uuid_t get_uuid() const;

  std::string to_string();

 protected:
  esp_bt_uuid_t uuid_;
  std::string string_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
