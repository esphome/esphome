#include "d01.h"
#include "esphome/core/log.h"

// uart specification for d01 sensor from https://manuals.plus/ae/1005006417362019:
//
// A frame of serial output data includes 4 bytes, formatted as follows:
// __Characteristic byte: Fixed value 0xA5.
// __Data byte: DATAH is the high 7 bits of the concentration value, and DATAL is the low 7 bits of the concentration
// value.
// __Check byte: The low 7 bits of the sum of all bytes before the check byte.
//
// If the serial output is 4 bytes of data: 0*A5 0*01 0*2C 0*52, then DATAH = 0*01 = 1, DATAL = 0*2C = 44.
// Concentration value = 1*128 + 44 = 172 µg/m³.
//
// The PM2.5 dust concentration value obtained from the dust sensor needs to be calibrated with a K value coefficient
// based on the TSI instrument's photometric method. It is generally recommended to use 0.4.

namespace esphome::d01 {

static const char *const TAG = "d01";

static const uint8_t D01_FRAME_HEADER = 0xA5;

void D01SensorComponent::dump_config() { LOG_SENSOR("  ", "D01 PM2.5", this); }

void D01SensorComponent::loop() {
  uint8_t buf[4];
  while (this->available() >= 4) {
    if (this->peek() != D01_FRAME_HEADER) {
      this->read();
      continue;
    }
    this->read_array(buf, 4);
    uint8_t sum = (buf[0] + buf[1] + buf[2]) & 0x7F;
    if (sum != buf[3]) {
      ESP_LOGW(TAG, "checksum mismatch");
      continue;
    }
    uint16_t latest_concentration = (buf[1] & 0x7F) * 128 + (buf[2] & 0x7F);
    ESP_LOGV(TAG, "Unadjusted PM2.5 Concentration: %d µg/m³", latest_concentration);
    this->publish_state(latest_concentration);
  }
}

}  // namespace esphome::d01
