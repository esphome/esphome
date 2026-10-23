#include "ds1603l.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ds1603l {

ESPHOME_LOG_TAG(TAG, "ds1603l.sensor");

void DS1603L::loop() {
  uint8_t byte;
  while (this->available() > 0 && this->read_byte(&byte)) {
    const uint8_t *frame = this->reader_.feed(byte);
    if (frame == nullptr) {
      if (this->reader_.length() == 0) {
        ESP_LOGV(TAG, "Skipping byte 0x%02X while looking for header", byte);
      }
      continue;
    }
    if (!this->parse_data_(frame)) {
      // The header byte was part of the payload of a misaligned frame, so realign instead of dropping everything
      this->reader_.resync();
    }
  }
}

void DS1603L::dump_config() { LOG_SENSOR("", "DS1603L", this); }

bool DS1603L::parse_data_(const uint8_t *frame) {
  uint8_t header = frame[0];
  uint8_t data_h = frame[1];
  uint8_t data_l = frame[2];
  uint8_t checksum = frame[3];

  uint8_t computed_checksum = (header + data_h + data_l) & 0xFF;

  ESP_LOGV(TAG, "Data: Header=0x%02X, Data_H=0x%02X, Data_L=0x%02X, Checksum=0x%02X", header, data_h, data_l, checksum);

  if (checksum != computed_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: received 0x%02X, expected 0x%02X", checksum, computed_checksum);
    return false;
  }

  this->publish_state(encode_uint16(data_h, data_l));
  return true;
}

}  // namespace esphome::ds1603l
