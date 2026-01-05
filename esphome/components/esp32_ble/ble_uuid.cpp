#include "ble_uuid.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_UUID

#include <cstring>
#include "esphome/core/helpers.h"

namespace esphome::esp32_ble {

ESPBTUUID ESPBTUUID::from_uuid(esp_bt_uuid_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = uuid.len;
  if (uuid.len == ESP_UUID_LEN_16) {
    ret.uuid_.uuid.uuid16 = uuid.uuid.uuid16;
  } else if (uuid.len == ESP_UUID_LEN_32) {
    ret.uuid_.uuid.uuid32 = uuid.uuid.uuid32;
  } else if (uuid.len == ESP_UUID_LEN_128) {
    memcpy(ret.uuid_.uuid.uuid128, uuid.uuid.uuid128, ESP_UUID_LEN_128);
  }
  return ret;
}
ESPBTUUID ESPBTUUID::as_128bit() const {
  if (this->uuid_.len == ESP_UUID_LEN_128) {
    return *this;
  }
  uint8_t data[] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint32_t uuid32;
  if (this->uuid_.len == ESP_UUID_LEN_32) {
    uuid32 = this->uuid_.uuid.uuid32;
  } else {
    uuid32 = this->uuid_.uuid.uuid16;
  }
  for (uint8_t i = 0; i < this->uuid_.len; i++) {
    data[12 + i] = ((uuid32 >> i * 8) & 0xFF);
  }
  return ESPBTUUID::from_raw(data);
}
bool ESPBTUUID::contains(uint8_t data1, uint8_t data2) const {
  if (this->uuid_.len == ESP_UUID_LEN_16) {
    return (this->uuid_.uuid.uuid16 >> 8) == data2 && (this->uuid_.uuid.uuid16 & 0xFF) == data1;
  } else if (this->uuid_.len == ESP_UUID_LEN_32) {
    for (uint8_t i = 0; i < 3; i++) {
      bool a = ((this->uuid_.uuid.uuid32 >> i * 8) & 0xFF) == data1;
      bool b = ((this->uuid_.uuid.uuid32 >> (i + 1) * 8) & 0xFF) == data2;
      if (a && b)
        return true;
    }
  } else {
    for (uint8_t i = 0; i < 15; i++) {
      if (this->uuid_.uuid.uuid128[i] == data1 && this->uuid_.uuid.uuid128[i + 1] == data2)
        return true;
    }
  }
  return false;
}
bool ESPBTUUID::operator==(const ESPBTUUID &uuid) const {
  if (this->uuid_.len == uuid.uuid_.len) {
    switch (this->uuid_.len) {
      case ESP_UUID_LEN_16:
        return this->uuid_.uuid.uuid16 == uuid.uuid_.uuid.uuid16;
      case ESP_UUID_LEN_32:
        return this->uuid_.uuid.uuid32 == uuid.uuid_.uuid.uuid32;
      case ESP_UUID_LEN_128:
        return memcmp(this->uuid_.uuid.uuid128, uuid.uuid_.uuid.uuid128, ESP_UUID_LEN_128) == 0;
      default:
        return false;
    }
  }
  return this->as_128bit() == uuid.as_128bit();
}
esp_bt_uuid_t ESPBTUUID::get_uuid() const { return this->uuid_; }
void ESPBTUUID::to_str(std::span<char, UUID_STR_LEN> output) const {
  char *pos = output.data();

  switch (this->uuid_.len) {
    case ESP_UUID_LEN_16:
      *pos++ = '0';
      *pos++ = 'x';
      *pos++ = format_hex_pretty_char(this->uuid_.uuid.uuid16 >> 12);
      *pos++ = format_hex_pretty_char((this->uuid_.uuid.uuid16 >> 8) & 0x0F);
      *pos++ = format_hex_pretty_char((this->uuid_.uuid.uuid16 >> 4) & 0x0F);
      *pos++ = format_hex_pretty_char(this->uuid_.uuid.uuid16 & 0x0F);
      *pos = '\0';
      return;

    case ESP_UUID_LEN_32:
      *pos++ = '0';
      *pos++ = 'x';
      for (int shift = 28; shift >= 0; shift -= 4) {
        *pos++ = format_hex_pretty_char((this->uuid_.uuid.uuid32 >> shift) & 0x0F);
      }
      *pos = '\0';
      return;

    default:
    case ESP_UUID_LEN_128:
      // Format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
      for (int8_t i = 15; i >= 0; i--) {
        uint8_t byte = this->uuid_.uuid.uuid128[i];
        *pos++ = format_hex_pretty_char(byte >> 4);
        *pos++ = format_hex_pretty_char(byte & 0x0F);
        if (i == 12 || i == 10 || i == 8 || i == 6) {
          *pos++ = '-';
        }
      }
      *pos = '\0';
      return;
  }
}
std::string ESPBTUUID::to_string() const {
  char buf[UUID_STR_LEN];
  this->to_str(buf);
  return std::string(buf);
}

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_UUID
#endif  // USE_ESP32
