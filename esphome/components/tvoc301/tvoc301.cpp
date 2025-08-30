#include "tvoc301.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tvoc301 {

static const char *const TAG = "TVOC-301";

static const uint8_t TVOC301_RESPONSE_HEADER[] = {0x2c, 0xe4};
static const size_t TVOC301_RESPONSE_LENGTH = 9;

void TVOC301Component::setup() {
  // because this implementation is currently rx-only, there is nothing to setup
  ESP_LOGV(TAG, "SETUP");
}

void TVOC301Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TVOC301:");
  LOG_SENSOR("  ", "eCO2", this->eco2_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "CH2O", this->ch2o_sensor_);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600);
}

void TVOC301Component::update() {
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      // finished
      this->parse_data_();
      this->data_index_ = 0;
      break;
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

void TVOC301Component::loop() {}

float TVOC301Component::get_setup_priority() const { return setup_priority::DATA; }

uint8_t TVOC301Component::tvoc301_checksum_() const {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < TVOC301_RESPONSE_LENGTH - 1; i++) {
    sum += this->data_[i];
  }
  return sum;
}

optional<bool> TVOC301Component::check_byte_() const {
  const uint8_t index = this->data_index_;
  const uint8_t byte = this->data_[index];

  // byte 0, 1 are the fixed header
  if (index < sizeof(TVOC301_RESPONSE_HEADER)) {
    return byte == TVOC301_RESPONSE_HEADER[index];
  }

  // byte 2, 3: TVOC hi, lo
  // byte 4, 5: CH2O hi, lo
  // byte 6, 7: eCO2 hi, lo
  if (index < TVOC301_RESPONSE_LENGTH - 1)
    return true;

  // byte 8: checksum
  if (index == TVOC301_RESPONSE_LENGTH - 1) {
    uint8_t checksum = tvoc301_checksum_();
    if (checksum != byte) {
      ESP_LOGW(TAG, "TVOC-301 checksum is wrong: %02x, expected %02x", checksum, byte);
      return false;
    }
    return {};
  }

  return false;
}

void TVOC301Component::parse_data_() {
  const uint16_t tvoc = this->get_16_bit_uint_(2);
  ESP_LOGD(TAG, "Got TVOC: %d µg/m³", tvoc);
  if (this->tvoc_sensor_ != nullptr) {
    this->tvoc_sensor_->publish_state(tvoc);
  }
  const uint16_t ch2o = this->get_16_bit_uint_(4);
  ESP_LOGD(TAG, "Got CH₂O: %d µg/m³", ch2o);
  if (this->ch2o_sensor_ != nullptr) {
    this->ch2o_sensor_->publish_state(ch2o);
  }
  const uint16_t eco2 = this->get_16_bit_uint_(6);
  ESP_LOGD(TAG, "Got eCO₂: %d ppm", eco2);
  if (this->eco2_sensor_ != nullptr) {
    this->eco2_sensor_->publish_state(eco2);
  }
}

}  // namespace tvoc301
}  // namespace esphome
