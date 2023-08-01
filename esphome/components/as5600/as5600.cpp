#include "as5600.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as5600 {

static const char *const TAG = "as5600";

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

void AS5600Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS5600...");

  if (!this->read_byte(REGISTER_STATUS).has_value()) {
    this->mark_failed();
    return;
  }

  // configuration direction pin, if given
  // the dir pin on the chip should be low for clockwise
  // and high for counterclockwise. If the pin is left floating
  // the reported positions will be erratic.
  if (this->dir_pin_ != nullptr) {
    this->dir_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->dir_pin_->digital_write(this->direction_ == 1);
  }

  // build config register
  // take the value, shift it left, and add mask to it to ensure we
  // are only changing the bits appropriate for that setting in the
  // off chance we somehow have bad value in there and it makes for
  // a nice visual for the bit positions.
  uint16_t config = 0;
  // clang-format off
  config |= (this->watchdog_      << 13) & 0b0010000000000000;
  config |= (this->fast_filter_   << 10) & 0b0001110000000000;
  config |= (this->slow_filter_   <<  8) & 0b0000001100000000;
  config |= (this->pwm_frequency_ <<  6) & 0b0000000011000000;
  config |= (this->output_mode_   <<  4) & 0b0000000000110000;
  config |= (this->hysteresis_    <<  2) & 0b0000000000001100;
  config |= (this->power_mode_    <<  0) & 0b0000000000000011;
  // clang-format on

  // write config to config register
  if (!this->write_byte_16(REGISTER_CONF, config)) {
    this->mark_failed();
    return;
  }

  // configure the start position
  this->write_byte_16(REGISTER_ZPOS, this->start_position_);

  // configure either end position or max angle
  if (this->end_mode_ == END_MODE_POSITION) {
    this->write_byte_16(REGISTER_MPOS, this->end_position_);
  } else {
    this->write_byte_16(REGISTER_MANG, this->end_position_);
  }

  // calculate the raw max from end position or start + range
  this->raw_max_ = this->end_mode_ == END_MODE_POSITION ? this->end_position_ & 4095
                                                        : (this->start_position_ + this->end_position_) & 4095;

  // calculate allowed range of motion by taking the start from the end
  // but only if the end is greater than the start. If the start is greater
  // than the end position, then that means we take the start all the way to
  // reset point (i.e. 0 deg raw) and then we that with the end position
  uint16_t range = this->raw_max_ > this->start_position_ ? this->raw_max_ - this->start_position_
                                                          : (4095 - this->start_position_) + this->raw_max_;

  // range scale is ratio of actual allowed range to the full range
  this->range_scale_ = range / 4095.0f;
}

void AS5600Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS5600:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS5600 failed!");
    return;
  }

  ESP_LOGCONFIG(TAG, "  Watchdog: %d", this->watchdog_);
  ESP_LOGCONFIG(TAG, "  Fast Filter: %d", this->fast_filter_);
  ESP_LOGCONFIG(TAG, "  Slow Filter: %d", this->slow_filter_);
  ESP_LOGCONFIG(TAG, "  Hysteresis: %d", this->hysteresis_);
  ESP_LOGCONFIG(TAG, "  Start Position: %d", this->start_position_);
  if (this->end_mode_ == END_MODE_POSITION) {
    ESP_LOGCONFIG(TAG, "  End Position: %d", this->end_position_);
  } else {
    ESP_LOGCONFIG(TAG, "  Range: %d", this->end_position_);
  }

  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Sensor", sensor);
    ESP_LOGCONFIG(TAG, "    Out of Range Mode: %u", sensor->get_out_of_range_mode());
  }
}

bool AS5600Component::in_range(uint16_t raw_position) {
  return this->raw_max_ > this->start_position_
             ? raw_position >= this->start_position_ && raw_position <= this->raw_max_
             : raw_position >= this->start_position_ || raw_position <= this->raw_max_;
}

AS5600MagnetStatus AS5600Component::read_magnet_status() {
  uint8_t status = this->reg(REGISTER_STATUS).get() >> 3 & 0b000111;
  return static_cast<AS5600MagnetStatus>(status);
}

optional<uint16_t> AS5600Component::read_position() {
  uint16_t pos = 0;
  if (!this->read_byte_16(REGISTER_ANGLE, &pos)) {
    return {};
  }
  return pos;
}

optional<uint16_t> AS5600Component::read_raw_position() {
  uint16_t pos = 0;
  if (!this->read_byte_16(REGISTER_ANGLE_RAW, &pos)) {
    return {};
  }
  return pos;
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
