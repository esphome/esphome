#include "ble_uuid.h"

#ifdef USE_ESP32

#include <cstring>
#include <cstdio>
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_ble {

static const char *const TAG = "esp32_ble";

ESPBTUUID::ESPBTUUID() : uuid_() {}
ESPBTUUID ESPBTUUID::from_uint16(uint16_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_16;
  ret.uuid_.uuid.uuid16 = uuid;
  return ret;
}
ESPBTUUID ESPBTUUID::from_uint32(uint32_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_32;
  ret.uuid_.uuid.uuid32 = uuid;
  return ret;
}
ESPBTUUID ESPBTUUID::from_raw(const uint8_t *data) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_128;
  for (size_t i = 0; i < ESP_UUID_LEN_128; i++)
    ret.uuid_.uuid.uuid128[i] = data[i];
  return ret;
}
ESPBTUUID ESPBTUUID::from_raw(const std::string &data) {
  ESPBTUUID ret;
  if (data.length() == 4) {
    ret.uuid_.len = ESP_UUID_LEN_16;
    ret.uuid_.uuid.uuid16 = 0;
    for (int i = 0; i < data.length();) {
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid16 += (((msb & 0x0F) << 4) | (lsb & 0x0F)) << (2 - i) * 4;
      i += 2;
    }
  } else if (data.length() == 8) {
    ret.uuid_.len = ESP_UUID_LEN_32;
    ret.uuid_.uuid.uuid32 = 0;
    for (int i = 0; i < data.length();) {
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid32 += (((msb & 0x0F) << 4) | (lsb & 0x0F)) << (6 - i) * 4;
      i += 2;
    }
  } else if (data.length() == 16) {  // how we can have 16 byte length string reprezenting 128 bit uuid??? needs to be
                                     // investigated (lack of time)
    ret.uuid_.len = ESP_UUID_LEN_128;
    memcpy(ret.uuid_.uuid.uuid128, (uint8_t *) data.data(), 16);
  } else if (data.length() == 36) {
    // If the length of the string is 36 bytes then we will assume it is a long hex string in
    // UUID format.
    ret.uuid_.len = ESP_UUID_LEN_128;
    int n = 0;
    for (int i = 0; i < data.length();) {
      if (data.c_str()[i] == '-')
        i++;
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid128[15 - n++] = ((msb & 0x0F) << 4) | (lsb & 0x0F);
      i += 2;
    }
  } else {
    ESP_LOGE(TAG, "ERROR: UUID value not 2, 4, 16 or 36 bytes - %s", data.c_str());
  }
  return ret;
}
ESPBTUUID ESPBTUUID::from_uuid(esp_bt_uuid_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = uuid.len;
  ret.uuid_.uuid.uuid16 = uuid.uuid.uuid16;
  ret.uuid_.uuid.uuid32 = uuid.uuid.uuid32;
  for (size_t i = 0; i < ESP_UUID_LEN_128; i++)
    ret.uuid_.uuid.uuid128[i] = uuid.uuid.uuid128[i];
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
        if (uuid.uuid_.uuid.uuid16 == this->uuid_.uuid.uuid16) {
          return true;
        }
        break;
      case ESP_UUID_LEN_32:
        if (uuid.uuid_.uuid.uuid32 == this->uuid_.uuid.uuid32) {
          return true;
        }
        break;
      case ESP_UUID_LEN_128:
        for (int i = 0; i < ESP_UUID_LEN_128; i++) {
          if (uuid.uuid_.uuid.uuid128[i] != this->uuid_.uuid.uuid128[i]) {
            return false;
          }
        }
        return true;
        break;
    }
  } else {
    return this->as_128bit() == uuid.as_128bit();
  }
  return false;
}
esp_bt_uuid_t ESPBTUUID::get_uuid() { return this->uuid_; }
std::string ESPBTUUID::to_string() {
  char sbuf[64];
  switch (this->uuid_.len) {
    case ESP_UUID_LEN_16:
      sprintf(sbuf, "0x%02X%02X", this->uuid_.uuid.uuid16 >> 8, this->uuid_.uuid.uuid16 & 0xff);
      break;
    case ESP_UUID_LEN_32:
      sprintf(sbuf, "0x%02X%02X%02X%02X", this->uuid_.uuid.uuid32 >> 24, (this->uuid_.uuid.uuid32 >> 16 & 0xff),
              (this->uuid_.uuid.uuid32 >> 8 & 0xff), this->uuid_.uuid.uuid32 & 0xff);
      break;
    default:
    case ESP_UUID_LEN_128:
      char *bpos = sbuf;
      for (int8_t i = 15; i >= 0; i--) {
        sprintf(bpos, "%02X", this->uuid_.uuid.uuid128[i]);
        bpos += 2;
        if (i == 6 || i == 8 || i == 10 || i == 12)
          sprintf(bpos++, "-");
      }
      sbuf[47] = '\0';
      break;
  }
  return sbuf;
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
