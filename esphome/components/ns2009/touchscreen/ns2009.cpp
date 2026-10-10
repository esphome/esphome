#include "ns2009.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ns2009 {

ESPHOME_LOG_TAG(TAG, "ns2009");

static constexpr uint8_t GET_X = 0xC0;
static constexpr uint8_t GET_Y = 0xD0;
static constexpr uint8_t GET_Z = 0xE2;

void NS2009Component::setup() {
  if (!this->read_byte(GET_Z).has_value()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

void NS2009Component::update_touches() {
  auto data_z = this->read_byte(GET_Z);
  if (!data_z.has_value()) {
    ESP_LOGW(TAG, "failed to read %s position, skipping update", LOG_STR_LITERAL("Z"));
    this->skip_update_ = true;
    return;
  }
  uint8_t z = *data_z;
  if (z <= this->threshold_) {
    return;
  }

  auto data_x = this->read_bytes<2>(GET_X);
  if (!data_x.has_value()) {
    ESP_LOGW(TAG, "failed to read %s position, skipping update", LOG_STR_LITERAL("X"));
    this->skip_update_ = true;
    return;
  }
  uint16_t x = encode_uint16((*data_x)[0], (*data_x)[1]) >> 4;  // 12 bit followed by 4 0's

  auto data_y = this->read_bytes<2>(GET_Y);
  if (!data_y.has_value()) {
    ESP_LOGW(TAG, "failed to read %s position, skipping update", LOG_STR_LITERAL("Y"));
    this->skip_update_ = true;
    return;
  }
  uint16_t y = encode_uint16((*data_y)[0], (*data_y)[1]) >> 4;  // 12 bit followed by 4 0's

  ESP_LOGV(TAG, "X %4d   Y %4d   Z %3d", x, y, z);
  this->add_raw_touch_position_(0, x, y, z);
}

void NS2009Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NS2009 Touchscreen:");
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::ns2009
