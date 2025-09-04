#include "ns2009.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ns2009 {

static const char *const TAG = "ns2009";

static const uint8_t PRIMARY_ADDRESS = 0x48;    // default I2C address for NS2009
static const uint8_t SECONDARY_ADDRESS = 0x49;  // secondary I2C address for NS2009
static const uint8_t GET_X[1] = {0xC0};
static const uint8_t GET_Y[1] = {0xD0};
static const uint8_t GET_Z[1] = {0xE2};

void NS2009Component::setup() {
  i2c::ErrorCode err = this->write(GET_Z, sizeof(GET_Z));

  if (err != i2c::ERROR_OK && this->address_ == PRIMARY_ADDRESS) {
    ESP_LOGD(TAG, "tried primary address 0x%02x, trying secondary address 0x%02x", PRIMARY_ADDRESS, SECONDARY_ADDRESS);
    this->address_ = SECONDARY_ADDRESS;
    err = this->write(GET_Z, sizeof(GET_Z));
  }

  if (err == i2c::ERROR_OK) {
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

  auto dataZ = this->read_byte(0xe2);
  if (dataZ.has_value()) {
    uint8_t z = *dataZ;

    if (z > this->threshold_) {
      auto dataX = this->read_bytes<2>(0xc0);
      uint16_t x = encode_uint16((*dataX)[0], (*dataX)[1]) >> 4;  // 12 bit followed by 4 0's
      auto dataY = this->read_bytes<2>(0xd0);
      uint16_t y = encode_uint16((*dataY)[0], (*dataY)[1]) >> 4;  // 12 bit followed by 4 0's

      ESP_LOGD(TAG, "X %4d   Y %4d   Z %3d", x, y, z);
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
