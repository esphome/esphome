#include "sds011.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sds011 {

static const char *TAG = "sds011";

static const uint8_t SDS011_MSG_REQUEST_LENGTH = 19;
static const uint8_t SDS011_MSG_RESPONSE_LENGTH = 10;
static const uint8_t SDS011_DATA_REQUEST_LENGTH = 15;
static const uint8_t SDS011_DATA_RESPONSE_LENGTH = 6;
static const uint8_t SDS011_MSG_HEAD = 0xaa;
static const uint8_t SDS011_MSG_TAIL = 0xab;
static const uint8_t SDS011_COMMAND_ID_REQUEST = 0xb4;
static const uint8_t SDS011_COMMAND_ID_RESPONSE = 0xc5;
static const uint8_t SDS011_COMMAND_ID_DATA = 0xc0;
static const uint8_t SDS011_COMMAND_REPORT_MODE = 0x02;
static const uint8_t SDS011_COMMAND_QUERY_DATA = 0x04;
static const uint8_t SDS011_COMMAND_SET_DEVICE_ID = 0x05;
static const uint8_t SDS011_COMMAND_SLEEP = 0x06;
static const uint8_t SDS011_COMMAND_FIRMWARE = 0x07;
static const uint8_t SDS011_COMMAND_PERIOD = 0x08;
static const uint8_t SDS011_GET_MODE = 0x00;
static const uint8_t SDS011_SET_MODE = 0x01;
static const uint8_t SDS011_MODE_REPORT_ACTIVE = 0x00;
static const uint8_t SDS011_MODE_REPORT_QUERY = 0x01;
static const uint8_t SDS011_MODE_SLEEP = 0x00;
static const uint8_t SDS011_MODE_WORK = 0x01;

void SDS011Component::setup() {
  if (this->rx_mode_only_) {
    // In RX-only mode we do not setup the sensor, it is assumed to be setup
    // already
    return;
  }
  uint8_t command_data[SDS011_DATA_REQUEST_LENGTH] = {0};
  command_data[0] = SDS011_COMMAND_REPORT_MODE;
  command_data[1] = SDS011_SET_MODE;
  command_data[2] = SDS011_MODE_REPORT_ACTIVE;
  command_data[13] = 0xff;
  command_data[14] = 0xff;
  this->sds011_write_command_(command_data);

  command_data[0] = SDS011_COMMAND_PERIOD;
  command_data[1] = SDS011_SET_MODE;
  command_data[2] = this->update_interval_min_;
  command_data[13] = 0xff;
  command_data[14] = 0xff;
  this->sds011_write_command_(command_data);
}

void SDS011Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SDS011:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %u min", this->update_interval_min_);
  ESP_LOGCONFIG(TAG, "  RX-only mode: %s", ONOFF(this->rx_mode_only_));
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  this->check_uart_settings(9600);
}

void SDS011Component::loop() {
  const uint32_t now = millis();
  if ((now - this->last_transmission_ >= 500) && this->data_index_) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGV(TAG, "Last transmission too long ago. Reset RX index.");
    this->data_index_ = 0;
  }

  if (this->available() == 0) {
    return;
  }

  this->last_transmission_ = now;
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
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

float SDS011Component::get_setup_priority() const { return setup_priority::DATA; }

void SDS011Component::set_rx_mode_only(bool rx_mode_only) { this->rx_mode_only_ = rx_mode_only; }

void SDS011Component::sds011_write_command_(const uint8_t *command_data) {
  this->write_byte(SDS011_MSG_HEAD);
  this->write_byte(SDS011_COMMAND_ID_REQUEST);
  this->write_array(command_data, SDS011_DATA_REQUEST_LENGTH);
  this->write_byte(sds011_checksum_(command_data, SDS011_DATA_REQUEST_LENGTH));
  this->write_byte(SDS011_MSG_TAIL);
}

uint8_t SDS011Component::sds011_checksum_(const uint8_t *command_data, uint8_t length) const {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}

optional<bool> SDS011Component::check_byte_() const {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  if (index == 0) {
    return byte == SDS011_MSG_HEAD;
  }

  if (index == 1) {
    return byte == SDS011_COMMAND_ID_DATA;
  }

  if ((index >= 2) && (index <= 7)) {
    return true;
  }

  if (index == 8) {
    // checksum is without checksum bytes
    uint8_t checksum = sds011_checksum_(this->data_ + 2, SDS011_DATA_RESPONSE_LENGTH);
    if (checksum != byte) {
      ESP_LOGW(TAG, "SDS011 Checksum doesn't match: 0x%02X!=0x%02X", byte, checksum);
      return false;
    }
    return true;
  }

  if (index == 9) {
    if (byte != SDS011_MSG_TAIL) {
      return false;
    }
  }

  return {};
}

void SDS011Component::parse_data_() {
  this->status_clear_warning();
  const float pm_2_5_concentration = this->get_16_bit_uint_(2) / 10.0f;
  const float pm_10_0_concentration = this->get_16_bit_uint_(4) / 10.0f;

  ESP_LOGD(TAG, "Got PM2.5 Concentration: %.1f µg/m³, PM10.0 Concentration: %.1f µg/m³", pm_2_5_concentration,
           pm_10_0_concentration);
  if (pm_2_5_concentration <= 0 && pm_10_0_concentration <= 0) {
    // not yet any valid data
    return;
  }
  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  }
  if (this->pm_10_0_sensor_ != nullptr) {
    this->pm_10_0_sensor_->publish_state(pm_10_0_concentration);
  }
}

uint16_t SDS011Component::get_16_bit_uint_(uint8_t start_index) const {
  return (uint16_t(this->data_[start_index + 1]) << 8) | uint16_t(this->data_[start_index]);
}
void SDS011Component::set_update_interval_min(uint8_t update_interval_min) {
  this->update_interval_min_ = update_interval_min;
}

}  // namespace sds011
}  // namespace esphome
