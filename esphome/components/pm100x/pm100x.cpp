#include "pm100x.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cmath>

namespace esphome {
namespace pm100x {

static const char *const TAG = "pm100x";

static const uint8_t PM1003_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM1003_COMMAND_MEASURE[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

static const uint8_t PM1006_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM1006_COMMAND_MEASURE[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

static const uint8_t PM1006K_RESPONSE_HEADER[] = {0x16, 0x0D, 0x02};
static const uint8_t PM1006K_COMMAND_MEASURE[] = {0x11, 0x01, 0x02, 0xEC};

static const char *model_to_string(PM100XModel model) {
  switch (model) {
    case PM100XModel::PM1003:
      return "pm1003";
    case PM100XModel::PM1006:
      return "pm1006";
    case PM100XModel::PM1006K:
      return "pm1006k";
    default:
      return "unknown";
  }
}

void PM100XComponent::setup() {
  this->start_time_ = App.get_loop_component_start_time();
  this->initial_delay_done_ = false;
  if (this->pwm_sensor_ != nullptr) {
    this->pwm_sensor_->add_on_state_callback([this](float duty_percent) { this->handle_pwm_state_(duty_percent); });
  }
}

void PM100XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PM100X:");
  ESP_LOGCONFIG(TAG, "  Model: %s", model_to_string(this->model_));
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "PWM Duty Percent", this->pwm_sensor_);
  LOG_UPDATE_INTERVAL(this);
  if (this->parent_ != nullptr) {
    this->check_uart_settings(9600);
  }
}

void PM100XComponent::update() {
  if (this->parent_ == nullptr || this->pwm_sensor_ != nullptr)
    return;
  if (this->get_update_interval() == 0)
    return;
  if (!this->initial_delay_done_) {
    const uint32_t now = App.get_loop_component_start_time();
    if (now - this->start_time_ < this->startup_delay_ms_) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  size_t command_length = 0;
  const uint8_t *command = this->get_command_measure_(command_length);
  if (command == nullptr || command_length == 0)
    return;
  ESP_LOGV(TAG, "sending measurement request");
  this->write_array(command, command_length);
}

void PM100XComponent::loop() {
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
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}

float PM100XComponent::get_setup_priority() const { return setup_priority::DATA; }

uint8_t PM100XComponent::pm100x_checksum_(const uint8_t *command_data, uint8_t length) const {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}

optional<bool> PM100XComponent::check_byte_() const {
  size_t header_length = 0;
  const uint8_t *header = this->get_response_header_(header_length);
  if (header == nullptr || header_length == 0)
    return false;
  const size_t data_length = this->get_frame_data_length_();
  const uint8_t index = this->data_index_;
  const uint8_t byte = this->data_[index];

  // index 0..2 are the fixed header
  if (index < header_length) {
    return byte == header[index];
  }

  if (index < (header_length + data_length))
    return true;

  // checksum
  if (index == (header_length + data_length)) {
    uint8_t checksum = pm100x_checksum_(this->data_, header_length + data_length + 1);
    if (checksum != 0) {
      ESP_LOGW(TAG, "PM100X checksum is wrong: %02x, expected zero", checksum);
      return false;
    }
    return {};
  }

  return false;
}

size_t PM100XComponent::get_frame_data_length_() const {
  switch (this->model_) {
    case PM100XModel::PM1006K:
      return 12;
    default:
      return 16;
  }
}

const uint8_t *PM100XComponent::get_response_header_(size_t &length) const {
  switch (this->model_) {
    case PM100XModel::PM1003:
      length = sizeof(PM1003_RESPONSE_HEADER);
      return PM1003_RESPONSE_HEADER;
    case PM100XModel::PM1006:
      length = sizeof(PM1006_RESPONSE_HEADER);
      return PM1006_RESPONSE_HEADER;
    case PM100XModel::PM1006K:
      length = sizeof(PM1006K_RESPONSE_HEADER);
      return PM1006K_RESPONSE_HEADER;
  }
  length = 0;
  return nullptr;
}

const uint8_t *PM100XComponent::get_command_measure_(size_t &length) const {
  switch (this->model_) {
    case PM100XModel::PM1003:
      length = sizeof(PM1003_COMMAND_MEASURE);
      return PM1003_COMMAND_MEASURE;
    case PM100XModel::PM1006:
      length = sizeof(PM1006_COMMAND_MEASURE);
      return PM1006_COMMAND_MEASURE;
    case PM100XModel::PM1006K:
      length = sizeof(PM1006K_COMMAND_MEASURE);
      return PM1006K_COMMAND_MEASURE;
  }
  length = 0;
  return nullptr;
}

float PM100XComponent::duty_to_pm25_(float duty_percent) const {
  if (duty_percent < 0.0f)
    duty_percent = 0.0f;
  if (duty_percent > 100.0f)
    duty_percent = 100.0f;

  switch (this->model_) {
    case PM100XModel::PM1006:
    case PM100XModel::PM1006K: {
      // PWM low-level ms maps to concentration (cycle 1000ms)
      float pm_2_5_concentration = (duty_percent * 10.0f) - 4.0f;
      if (pm_2_5_concentration < 0.0f)
        pm_2_5_concentration = 0.0f;
      if (pm_2_5_concentration > 992.0f)
        pm_2_5_concentration = 992.0f;
      return pm_2_5_concentration;
    }
    case PM100XModel::PM1003:
    default: {
      float pm_2_5_concentration = (duty_percent / 100.0f) * 500.0f;
      if (pm_2_5_concentration < 0.0f)
        pm_2_5_concentration = 0.0f;
      if (pm_2_5_concentration > 500.0f)
        pm_2_5_concentration = 500.0f;
      return pm_2_5_concentration;
    }
  }
}

void PM100XComponent::parse_data_() {
  size_t header_length = 0;
  const uint8_t *header = this->get_response_header_(header_length);
  if (header == nullptr || header_length == 0)
    return;

  switch (this->model_) {
    case PM100XModel::PM1003: {
      const uint16_t raw_duty = this->get_16_bit_uint_(5);
      float duty_percent = raw_duty / 10.0f;
      float pm_2_5_concentration = this->duty_to_pm25_(duty_percent);

      ESP_LOGD(TAG, "Got duty %.1f%%, PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);

      if (this->pm_2_5_sensor_ != nullptr) {
        this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
      }
      break;
    }
    case PM100XModel::PM1006: {
      const uint16_t pm_2_5 = this->get_16_bit_uint_(5);
      ESP_LOGD(TAG, "PM2.5 %u µg/m³", pm_2_5);
      if (this->pm_2_5_sensor_ != nullptr) {
        this->pm_2_5_sensor_->publish_state(pm_2_5);
      }
      break;
    }
    case PM100XModel::PM1006K: {
      const uint16_t pm_2_5 = this->get_16_bit_uint_(5);
      const uint16_t pm_1_0 = this->get_16_bit_uint_(9);
      const uint16_t pm_10_0 = this->get_16_bit_uint_(13);
      ESP_LOGD(TAG, "PM1.0 %u µg/m³, PM2.5 %u µg/m³, PM10.0 %u µg/m³", pm_1_0, pm_2_5, pm_10_0);

      if (this->pm_2_5_sensor_ != nullptr) {
        this->pm_2_5_sensor_->publish_state(pm_2_5);
      }
      if (this->pm_1_0_sensor_ != nullptr) {
        this->pm_1_0_sensor_->publish_state(pm_1_0);
      }
      if (this->pm_10_0_sensor_ != nullptr) {
        this->pm_10_0_sensor_->publish_state(pm_10_0);
      }
      break;
    }
  }
}

void PM100XComponent::handle_pwm_state_(float duty_percent) {
  if (!this->initial_delay_done_) {
    const uint32_t now = App.get_loop_component_start_time();
    if (now - this->start_time_ < this->startup_delay_ms_) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  if (this->pm_2_5_sensor_ == nullptr)
    return;
  if (std::isnan(duty_percent) || duty_percent < 0.0f)
    return;
  float pm_2_5_concentration = this->duty_to_pm25_(duty_percent);

  ESP_LOGD(TAG, "PWM duty %.1f%% -> PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);
  this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
}

}  // namespace pm100x
}  // namespace esphome
