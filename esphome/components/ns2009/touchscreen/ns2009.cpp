#include "ns2009.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

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
    ESP_LOGW(TAG, "failed to read %s, skipping update", LOG_STR_LITERAL("pressure"));
    this->skip_update_ = true;
    return;
  }
  uint8_t z = *data_z;
  if (z <= this->threshold_) {
    return;
  }

  auto x = this->read_axis_(GET_X);
  if (!x.has_value()) {
    ESP_LOGW(TAG, "failed to read %s, skipping update", LOG_STR_LITERAL("X position"));
    this->skip_update_ = true;
    return;
  }
  auto y = this->read_axis_(GET_Y);
  if (!y.has_value()) {
    ESP_LOGW(TAG, "failed to read %s, skipping update", LOG_STR_LITERAL("Y position"));
    this->skip_update_ = true;
    return;
  }

  ESP_LOGV(TAG, "X %4d   Y %4d   Z %3d", *x, *y, z);
  this->add_raw_touch_position_(0, *x, *y, z);
}

optional<uint16_t> NS2009Component::read_axis_(uint8_t cmd) {
  auto data = this->read_bytes<2>(cmd);
  if (!data.has_value()) {
    return {};
  }
  return encode_uint16((*data)[0], (*data)[1]) >> 4;  // 12 bit followed by 4 0's
}

void NS2009Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "NS2009 Touchscreen:\n"
                "  Threshold: %u",
                this->threshold_);
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::ns2009
