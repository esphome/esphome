#include "as5600_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as5600 {

static const char *const TAG = "as5600.sensor";

// Configuration registers
static const uint8_t REGISTER_ZMCO = 0x00;  // 8 bytes  / R
static const uint8_t REGISTER_ZPOS = 0x01;  // 16 bytes / RW
static const uint8_t REGISTER_MPOS = 0x03;  // 16 bytes / RW
static const uint8_t REGISTER_MANG = 0x05;  // 16 bytes / RW
static const uint8_t REGISTER_CONF = 0x07;  // 16 bytes / RW

// Output registers
static const uint8_t REGISTER_ANGLE_RAW = 0x0C;  // 16 bytes / R
static const uint8_t REGISTER_ANGLE = 0x0E;      // 16 bytes / R

// Status registers
static const uint8_t REGISTER_STATUS = 0x0B;     // 8 bytes  / R
static const uint8_t REGISTER_AGC = 0x1A;        // 8 bytes  / R
static const uint8_t REGISTER_MAGNITUDE = 0x1B;  // 16 bytes / R

float AS5600Sensor::get_setup_priority() const { return setup_priority::DATA; }

void AS5600Sensor::dump_config() {
  LOG_SENSOR("", "AS5600 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Out of Range Mode: %u", this->out_of_range_mode_);
  if (this->angle_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Angle Sensor", this->angle_sensor_);
  }
  if (this->raw_angle_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Raw Angle Sensor", this->raw_angle_sensor_);
  }
  if (this->position_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Position Sensor", this->position_sensor_);
  }
  if (this->raw_position_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Raw Position Sensor", this->raw_position_sensor_);
  }
  if (this->gain_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Gain Sensor", this->gain_sensor_);
  }
  if (this->magnitude_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Magnitude Sensor", this->magnitude_sensor_);
  }
  if (this->status_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Status Sensor", this->status_sensor_);
  }
  LOG_UPDATE_INTERVAL(this);
}

void AS5600Sensor::update() {
  if (this->gain_sensor_ != nullptr) {
    this->gain_sensor_->publish_state(this->parent_->reg(REGISTER_AGC).get());
  }

  if (this->magnitude_sensor_ != nullptr) {
    uint16_t value = 0;
    this->parent_->read_byte_16(REGISTER_MAGNITUDE, &value);
    this->magnitude_sensor_->publish_state(value);
  }

  // 2 = magnet not detected
  // 4 = magnet just right
  // 5 = magnet too strong
  // 6 = magnet too weak
  if (this->status_sensor_ != nullptr) {
    this->status_sensor_->publish_state(this->parent_->read_magnet_status());
  }

  auto pos = this->parent_->read_position();
  if (!pos.has_value()) {
    this->status_set_warning();
    return;
  }

  auto raw = this->parent_->read_raw_position();
  if (!raw.has_value()) {
    this->status_set_warning();
    return;
  }

  if (this->out_of_range_mode_ == OUT_RANGE_MODE_NAN) {
    this->publish_state(this->parent_->in_range(raw.value()) ? pos.value() : NAN);
  } else {
    this->publish_state(pos.value());
  }

  if (this->raw_position_sensor_ != nullptr) {
    this->raw_position_sensor_->publish_state(raw.value());
  }
  this->status_clear_warning();
}

}  // namespace as5600
}  // namespace esphome
