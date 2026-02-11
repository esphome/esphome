#include "pm100x_uart.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::pm100x_uart {

static const char *const TAG = "pm100x_uart";

// PM1003 and PM1006 share the same protocol
static const uint8_t PM100X_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM100X_COMMAND_MEASURE[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

// PM1006K has a different protocol
static const uint8_t PM1006K_RESPONSE_HEADER[] = {0x16, 0x0D, 0x02};
static const uint8_t PM1006K_COMMAND_MEASURE[] = {0x11, 0x01, 0x02, 0xEC};

void PM100XComponentUART::setup() { pm100x::PM100XComponent::setup(); }

void PM100XComponentUART::dump_config() {
  pm100x::PM100XComponent::dump_config();
  this->check_uart_settings(9600);
}

void PM100XComponentUART::update() {
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

void PM100XComponentUART::loop() {
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

uint8_t PM100XComponentUART::pm100x_checksum_(const uint8_t *command_data, size_t length) const {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}

optional<bool> PM100XComponentUART::check_byte_() const {
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

size_t PM100XComponentUART::get_frame_data_length_() const {
  switch (this->model_) {
    case pm100x::PM100XModel::PM1006K:
      return 12;
    default:
      return 16;
  }
}

const uint8_t *PM100XComponentUART::get_response_header_(size_t &length) const {
  switch (this->model_) {
    case pm100x::PM100XModel::PM1003:
    case pm100x::PM100XModel::PM1006:
      length = sizeof(PM100X_RESPONSE_HEADER);
      return PM100X_RESPONSE_HEADER;
    case pm100x::PM100XModel::PM1006K:
      length = sizeof(PM1006K_RESPONSE_HEADER);
      return PM1006K_RESPONSE_HEADER;
  }
  length = 0;
  return nullptr;
}

const uint8_t *PM100XComponentUART::get_command_measure_(size_t &length) const {
  switch (this->model_) {
    case pm100x::PM100XModel::PM1003:
    case pm100x::PM100XModel::PM1006:
      length = sizeof(PM100X_COMMAND_MEASURE);
      return PM100X_COMMAND_MEASURE;
    case pm100x::PM100XModel::PM1006K:
      length = sizeof(PM1006K_COMMAND_MEASURE);
      return PM1006K_COMMAND_MEASURE;
  }
  length = 0;
  return nullptr;
}

void PM100XComponentUART::parse_data_() {
  size_t header_length = 0;
  const uint8_t *header = this->get_response_header_(header_length);
  if (header == nullptr || header_length == 0)
    return;

  switch (this->model_) {
    case pm100x::PM100XModel::PM1003: {
      const uint16_t raw_duty = this->get_16_bit_uint_(5);
      float duty_percent = raw_duty / 10.0f;
      float pm_2_5_concentration = this->duty_to_pm25_(duty_percent);

      ESP_LOGD(TAG, "Got duty %.1f%%, PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);

      if (this->pm_2_5_sensor_ != nullptr) {
        this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
      }
      break;
    }
    case pm100x::PM100XModel::PM1006: {
      const uint16_t pm_2_5 = this->get_16_bit_uint_(5);
      ESP_LOGD(TAG, "PM2.5 %u µg/m³", pm_2_5);
      if (this->pm_2_5_sensor_ != nullptr) {
        this->pm_2_5_sensor_->publish_state(pm_2_5);
      }
      break;
    }
    case pm100x::PM100XModel::PM1006K: {
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

}  // namespace esphome::pm100x_uart
