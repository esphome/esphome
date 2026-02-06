#include "pm1003.h"
#include "esphome/core/log.h"

#include <cmath>
#include <string>

namespace esphome {
namespace pm1003 {

static const char *const TAG = "pm1003";

static const uint8_t PM1003_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM1003_COMMAND_MEASURE[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

void PM1003Component::setup() {
  this->start_time_ = millis();
  this->initial_delay_done_ = false;
  if (this->pwm_sensor_ != nullptr) {
    this->pwm_sensor_->add_on_state_callback([this](float pulse_width_s) { this->handle_pwm_state_(pulse_width_s); });
  }
}

void PM1003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM1003:");
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PWM Duty Percent", this->pwm_sensor_);
  LOG_UPDATE_INTERVAL(this);
  if (this->parent_ != nullptr) {
    this->check_uart_settings(9600);
  }
}

void PM1003Component::update() {
  if (this->parent_ == nullptr || this->pwm_sensor_ != nullptr)
    return;
  if (!this->initial_delay_done_) {
    if (millis() - this->start_time_ < this->startup_delay_ms_) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  ESP_LOGV(TAG, "sending measurement request");
  ESP_LOGI(TAG, "command payload bytes: 11 02 0B 01 E1");
  this->write_array(PM1003_COMMAND_MEASURE, sizeof(PM1003_COMMAND_MEASURE));
}

void PM1003Component::loop() {
  if (this->parent_ == nullptr || this->pwm_sensor_ != nullptr)
    return;
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      ESP_LOGV(TAG, "frame complete, len=%u", this->data_index_ + 1);
      // finished
      this->parse_data_();
      this->data_index_ = 0;
    } else if (!*check) {
      // wrong data
      ESP_LOGV(TAG, "Byte %i of received data frame is invalid.", this->data_index_);
      std::string hex;
      hex.reserve((this->data_index_ + 1) * 3);
      char buf[4];
      for (size_t i = 0; i <= this->data_index_; i++) {
        snprintf(buf, sizeof(buf), "%02X ", this->data_[i]);
        hex.append(buf);
      }
      ESP_LOGI(TAG, "invalid frame bytes: %s", hex.c_str());
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}

float PM1003Component::get_setup_priority() const { return setup_priority::DATA; }

uint8_t PM1003Component::pm1003_checksum_(const uint8_t *command_data, uint8_t length) const {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}

optional<bool> PM1003Component::check_byte_() const {
  const uint8_t index = this->data_index_;
  const uint8_t byte = this->data_[index];

  // index 0..2 are the fixed header
  if (index < sizeof(PM1003_RESPONSE_HEADER)) {
    return byte == PM1003_RESPONSE_HEADER[index];
  }

  // just some additional notes here:
  // index 3..4 is unused
  // index 5..6 is our duty-based PM2.5 reading (3..6 is called DF1-DF4 in the datasheet)
  // the frame goes on up to DF16; we currently only consume DF3/DF4 for duty
  if (index < (sizeof(PM1003_RESPONSE_HEADER) + 16))
    return true;

  // checksum
  if (index == (sizeof(PM1003_RESPONSE_HEADER) + 16)) {
    uint8_t checksum = pm1003_checksum_(this->data_, sizeof(PM1003_RESPONSE_HEADER) + 17);
    if (checksum != 0) {
      ESP_LOGW(TAG, "PM1003 checksum is wrong: %02x, expected zero", checksum);
      return false;
    }
    return {};
  }

  return false;
}

void PM1003Component::parse_data_() {
  std::string hex;
  hex.reserve(sizeof(this->data_) * 3);
  char buf[4];
  for (unsigned char i : this->data_) {
    snprintf(buf, sizeof(buf), "%02X ", i);
    hex.append(buf);
  }
  ESP_LOGI(TAG, "received frame bytes: %s", hex.c_str());

  const uint16_t raw_duty = this->get_16_bit_uint_(5);
  float duty_percent = raw_duty / 10.0f;
  float pm_2_5_concentration = (duty_percent / 100.0f) * 500.0f;
  if (pm_2_5_concentration < 0.0f)
    pm_2_5_concentration = 0.0f;
  if (pm_2_5_concentration > 500.0f)
    pm_2_5_concentration = 500.0f;

  ESP_LOGD(TAG, "Got duty %.1f%%, PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);

  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  }
}

void PM1003Component::handle_pwm_state_(float duty_percent) {
  if (!this->initial_delay_done_) {
    if (millis() - this->start_time_ < this->startup_delay_ms_) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  if (this->pm_2_5_sensor_ == nullptr)
    return;
  if (std::isnan(duty_percent) || duty_percent < 0.0f)
    return;

  if (duty_percent > 100.0f)
    duty_percent = 100.0f;

  float pm_2_5_concentration = (duty_percent / 100.0f) * 500.0f;
  if (pm_2_5_concentration < 0.0f)
    pm_2_5_concentration = 0.0f;
  if (pm_2_5_concentration > 500.0f)
    pm_2_5_concentration = 500.0f;

  ESP_LOGD(TAG, "PWM duty %.1f%% -> PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);
  this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
}

}  // namespace pm1003
}  // namespace esphome
