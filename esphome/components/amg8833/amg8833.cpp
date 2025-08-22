#include "amg8833.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace amg8833 {

static const uint8_t FPSC = 0x02;
static const uint8_t INTC = 0x03;
static const uint8_t STAT = 0x04;
static const uint8_t AVE = 0x07;
static const uint8_t INTHL = 0x08;
static const uint8_t INTLL = 0x0A;
static const uint8_t INTSL = 0x0C;
static const uint8_t TTHL = 0x0E;
static const uint8_t INT0 = 0x10;
static const uint8_t SAM = 0x1F;
static const uint8_t T01L = 0x80;

static const char *const TAG = "amg8833";

void AMG8833::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  if (!this->write_fps_() || !this->write_filter_() || !this->write_mode_()) {
    this->mark_failed();
    return;
  }

  if (this->mode_ == MOTION && !this->write_motion_thresholds_()) {
    this->mark_failed();
    return;
  }

  if (this->mode_ == PRESENCE && !this->write_presence_thresholds_()) {
    this->mark_failed();
    return;
  }

#ifdef USE_NUMBER
  if (this->presence_hysteresis_number_ != nullptr)
    this->presence_hysteresis_number_->publish_state(this->presence_hysteresis_);
  if (this->presence_upper_number_ != nullptr)
    this->presence_upper_number_->publish_state(this->presence_upper_);
  if (this->presence_lower_number_ != nullptr)
    this->presence_lower_number_->publish_state(this->presence_lower_);
  if (this->motion_hysteresis_number_ != nullptr)
    this->motion_hysteresis_number_->publish_state(this->motion_hysteresis_);
  if (this->motion_maximum_number_ != nullptr)
    this->motion_maximum_number_->publish_state(this->motion_maximum_);
  if (this->motion_minimum_number_ != nullptr)
    this->motion_minimum_number_->publish_state(this->motion_minimum_);
#endif
#ifdef USE_SELECT
  if (this->fps_select_ != nullptr)
    this->fps_select_->publish_state(to_string(this->fps_));
  if (this->mode_select_ != nullptr)
    this->mode_select_->publish_state(to_string(this->mode_));
#endif
#ifdef USE_SWITCH
  if (this->filter_switch_ != nullptr)
    this->filter_switch_->publish_state(this->filter_);
  if (this->interrupt_pin_switch_ != nullptr)
    this->interrupt_pin_switch_->publish_state(this->interrupt_pin_);
#endif
}

void AMG8833::dump_config() {
  ESP_LOGCONFIG(TAG,
                "AMG8833:\n"
                "  Interrupt Pin: %s\n"
                "  Filter: %s\n"
                "  %s\n"
                "  Mode: %s\n"
                "  Software Output: %s\n"
                "  Presence Thresholds:\n"
                "    Upper: %.2f\n"
                "    Lower: %.2f\n"
                "    Hysteresis: %.2f\n"
                "  Motion Thresholds:\n"
                "    Maximum: %.2f\n"
                "    Minimum: %.2f\n"
                "    Hysteresis: %.2f\n",
                this->interrupt_pin_ ? "Enabled" : "Disabled", this->filter_ ? "Enabled" : "Disabled",
                to_string(this->fps_), to_string(this->mode_), this->software_output_ ? "Enabled" : "Disabled",
                this->presence_upper_, this->presence_lower_, this->presence_hysteresis_, this->motion_maximum_,
                this->motion_minimum_, this->motion_hysteresis_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed())
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
}

void AMG8833::update() {
  this->read_bytes(T01L, this->pixels_, 128);
  this->read_bytes(TTHL, this->thermistor_, 2);
  this->read_bytes(INT0, this->interrupts_, 8);
  this->read_bytes(STAT, &status_, 1);

  float maximum_temperature_ = __FLT_MIN__;
  float minimum_temperature_ = __FLT_MAX__;
  int index = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      float t = (this->pixels_[index++] | (this->pixels_[index++] << 8)) * 0.25f;
      measurement_[y][x] = t;
      if (t > maximum_temperature_)
        maximum_temperature_ = t;
      if (t < minimum_temperature_)
        minimum_temperature_ = t;
    }
  }

  this->measurement_callback_(measurement_);
#ifdef USE_BINARY_SENSOR
  if (this->motion_binary_sensor_ != nullptr && mode_ == MOTION && !this->software_output_)
    this->motion_binary_sensor_->publish_state(is_temperature_interrupt());
  if (this->presence_binary_sensor_ != nullptr && mode_ == PRESENCE && !this->software_output_)
    this->presence_binary_sensor_->publish_state(is_temperature_interrupt());
#endif
#ifdef USE_SENSOR
  if (this->ambient_sensor_ != nullptr)
    this->ambient_sensor_->publish_state(thermistor_to_temperature_());
  if (this->maximum_sensor_ != nullptr)
    this->maximum_sensor_->publish_state(maximum_temperature_);
  if (this->minimum_sensor_ != nullptr)
    this->minimum_sensor_->publish_state(minimum_temperature_);
#endif
}

void AMG8833::number_presence_hysteresis(float value) {
  this->presence_hysteresis_ = value;
  this->write_presence_thresholds_();
}

void AMG8833::number_presence_upper(float value) {
  this->presence_upper_ = value;
  this->write_presence_thresholds_();
}

void AMG8833::number_presence_lower(float value) {
  this->presence_lower_ = value;
  this->write_presence_thresholds_();
}

void AMG8833::number_motion_hysteresis(float value) {
  this->motion_hysteresis_ = value;
  this->write_motion_thresholds_();
}

void AMG8833::number_motion_maximum(float value) {
  this->motion_maximum_ = value;
  this->write_motion_thresholds_();
}

void AMG8833::number_motion_minimum(float value) {
  this->motion_minimum_ = value;
  this->write_motion_thresholds_();
}

void AMG8833::switch_filter(bool enable) {
  this->set_filter(enable);
  this->write_filter_();
}

void AMG8833::switch_interrupt_pin(bool enable) {
  this->set_interrupt_pin(enable);
  this->write_mode_();
}

void AMG8833::select_fps(const std::string &fps) {
  if (fps == "FPS_10")
    this->set_fps(FPS_10);

  if (fps == "FPS_1")
    this->set_fps(FPS_1);

  this->write_fps_();
  this->stop_poller();
  this->start_poller();
}

void AMG8833::select_mode(const std::string &mode) {
  if (mode == "MOTION") {
    this->set_mode(MOTION);
    this->write_motion_thresholds_();
  }

  if (mode == "PRESENCE") {
    this->set_mode(PRESENCE);
    this->write_presence_thresholds_();
  }

  this->write_mode_();
}

bool AMG8833::write_fps_() { return this->write_byte(FPSC, this->fps_); }

bool AMG8833::write_filter_() {
  return this->write_byte(SAM, 0x50) && this->write_byte(SAM, 0x45) && this->write_byte(SAM, 0x57) &&
         this->write_byte(AVE, this->filter_ ? 0x20 : 0x00) && this->write_byte(SAM, 0x00);
}

bool AMG8833::write_mode_() {
  uint8_t value = this->mode_ == PRESENCE ? 0x02 : 0x00;
  value |= interrupt_pin_ ? 0x01 : 0x00;
  return this->write_byte(INTC, value);
}

bool AMG8833::write_presence_thresholds_() {
  return this->write_threshold_(INTHL, this->presence_upper_) && this->write_threshold_(INTLL, this->presence_lower_) &&
         this->write_threshold_(INTSL, this->presence_hysteresis_);
}

bool AMG8833::write_motion_thresholds_() {
  return this->write_threshold_(INTHL, this->motion_maximum_) && this->write_threshold_(INTLL, this->motion_minimum_) &&
         this->write_threshold_(INTSL, this->motion_hysteresis_);
}

bool AMG8833::write_threshold_(uint8_t a_register, float temperature) {
  int16_t threshold = static_cast<int16_t>(temperature * 4);
  return this->write_byte(a_register, threshold & 0xFF) && this->write_byte(a_register + 1, (threshold >> 8) & 0xFF);
}

bool AMG8833::is_temperature_interrupt() { return this->status_ & 0x02; }

float AMG8833::thermistor_to_temperature_() {
  int16_t raw = this->thermistor_[0] | (this->thermistor_[1] << 8);
  if (raw & 0x800)
    raw |= 0xF000;

  return raw * 0.0625f;
}

}  // namespace amg8833
}  // namespace esphome
