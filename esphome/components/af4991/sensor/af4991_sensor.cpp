#include "af4991_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace af4991 {

static const char *const TAG = "af4991.sensor";

void AF4991Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up AF4991 Encoder Sensor..."); }

void AF4991Sensor::dump_config() {
  LOG_SENSOR("", "AF4991 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Invert: %s", this->invert_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Clockwise Rotation before Trigger: %isteps", this->clockwise_steps_before_trigger_);
  ESP_LOGCONFIG(TAG, "  Anticlockwise Rotation before Trigger: %isteps", this->anticlockwise_steps_before_trigger_);
}

/**
 * @brief Checks to ensure the read sensor value is within the limits set by the user
 *
 * @param position position to reference from
 * @return int32_t
 */
int32_t AF4991Sensor::check_position_within_limit_(int32_t position) {
  if (position > this->max_value_) {
    this->set_sensor_value(this->invert_ ? ~this->max_value_ + 1 : this->max_value_);
    position = this->max_value_;

    return position;
  }

  if (position < this->min_value_) {
    this->set_sensor_value(this->invert_ ? ~this->min_value_ + 1 : this->min_value_);
    position = this->min_value_;

    return position;
  }

  return position;
}

void AF4991Sensor::loop() {
  uint32_t unsigned_new_position = 0;
  int32_t new_position = 0;
  int32_t unchecked_new_position = 0;

  // Retrieve encoder position
  i2c::ErrorCode error = this->parent_->read_32(
      (uint16_t) adafruit_seesaw::SEESAW_ENCODER_BASE << 8 | adafruit_seesaw::SEESAW_ENCODER_POSITION,
      &unsigned_new_position);

  // Convert to int32_t
  new_position = check_position_within_limit_(this->invert_ ? (int32_t) ~unsigned_new_position + 1
                                                            : (int32_t) unsigned_new_position);
  unchecked_new_position = this->invert_ ? (int32_t) ~unsigned_new_position + 1 : (int32_t) unsigned_new_position;

  // Check to see if there was a error
  if (error != i2c::ERROR_OK) {
    return;
  }

  // Calculate encoder rotation direction
  // Clockwise Directional Movements
  if (unchecked_new_position > this->position_) {
    this->clockwise_move_ += 1;
    this->anticlockwise_move_ = 0;

    // Check to see if made movement is enough to trigger the automation trigger
    if (this->clockwise_move_ >= this->clockwise_steps_before_trigger_) {
      this->clockwise_move_ = 0;

      this->on_clockwise_callback_.call();
    }

    // Anticlockwise Directional Movements
  } else if (unchecked_new_position < this->position_) {
    this->anticlockwise_move_ += 1;
    this->clockwise_move_ = 0;

    // Check to see if made movement is enough to trigger the automation trigger
    if (this->anticlockwise_move_ >= this->anticlockwise_steps_before_trigger_) {
      this->anticlockwise_move_ = 0;

      this->on_anticlockwise_callback_.call();
    }
  }

  // Check to see if positional value has changed
  if (this->position_ != new_position || this->publish_initial_value_) {
    // Publish new state
    this->publish_state(new_position);

    // Updates stored position
    this->position_ = new_position;
    this->publish_initial_value_ = false;
  }
}

}  // namespace af4991
}  // namespace esphome
