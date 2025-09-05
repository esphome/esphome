#include "ns2009.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ns2009 {

static const char *const TAG = "ns2009";

static const uint8_t PRIMARY_ADDRESS = 0x48;
static const uint8_t SECONDARY_ADDRESS = 0x49;
static const uint8_t GET_X = 0xC0;
static const uint8_t GET_Y = 0xD0;
static const uint8_t GET_Z = 0xE2;

void NS2009Component::setup() {
  auto dataZ = this->read_byte(GET_Z);

  if (!dataZ.has_value() && this->address_ == PRIMARY_ADDRESS) {
    ESP_LOGD(TAG, "tried primary address 0x%02x with no success, trying secondary address 0x%02x", PRIMARY_ADDRESS,
             SECONDARY_ADDRESS);
    this->address_ = SECONDARY_ADDRESS;
    dataZ = this->read_byte(GET_Z);
  }

  if (dataZ.has_value()) {
    ESP_LOGD(TAG, "successfully connected with address 0x%02x", this->address_);
  } else {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  this->setup_done_ = true;
}

void NS2009Component::update_touches() {
  this->skip_update_ = true;  // skip send touch events by default, set to false after successful error checks
  if (!this->setup_done_) {
    return;
  }

  auto dataZ = this->read_byte(GET_Z);
  if (dataZ.has_value()) {
    uint8_t z = *dataZ;

    if (z > this->threshold_) {
      auto dataX = this->read_bytes<2>(GET_X);
      uint16_t x = encode_uint16((*dataX)[0], (*dataX)[1]) >> 4;  // 12 bit followed by 4 0's
      auto dataY = this->read_bytes<2>(GET_Y);
      uint16_t y = encode_uint16((*dataY)[0], (*dataY)[1]) >> 4;  // 12 bit followed by 4 0's

      ESP_LOGV(TAG, "X %4d   Y %4d   Z %3d", x, y, z);
      this->add_raw_touch_position_(0, x, y, z);
      this->skip_update_ = false;
    }
  }
}

void NS2009Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NS2009 Touchscreen:");
  LOG_I2C_DEVICE(this);
}

}  // namespace ns2009
}  // namespace esphome
