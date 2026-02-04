#include "pm1003.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pm1003 {

static const char *const TAG = "pm1003";

static const uint8_t PM1003_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM1003_REQUEST[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

void PM1003Component::setup() {
  this->start_time_ = millis();
  this->initial_delay_done_ = false;
}

void PM1003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM1003:");
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600);
}

void PM1003Component::update() {
  if (!this->initial_delay_done_) {
    if (millis() - this->start_time_ < 60000) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  ESP_LOGV(TAG, "sending measurement request");
  this->write_array(PM1003_REQUEST, sizeof(PM1003_REQUEST));
}

void PM1003Component::loop() {
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

}  // namespace pm1003
}  // namespace esphome
