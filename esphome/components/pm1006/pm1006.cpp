#include "pm1006.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pm1006 {

static const char *const TAG = "pm1006";

static const uint8_t PM1006_RESPONSE_HEADER[] = { 0x16, 0x11, 0x0B };

void PM1006Component::setup() {
  // because this implementation is currently rx-only, there is nothing to setup
}

void PM1006Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM1006:");
  //ESP_LOGCONFIG(TAG, "  RX-only mode: %s", ONOFF(this->rx_mode_only_));
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  this->check_uart_settings(9600);
}

void PM1006Component::loop() {
//  ESP_LOGD(TAG, "loop() called, sizeof header=%d", sizeof(PM1006_RESPONSE_HEADER));
  if (this->available() == 0) {
    return;
  }

//   ESP_LOGD(TAG, "data available");
  // this->last_transmission_ = now;
  while (this->available() != 0) {
//   ESP_LOGD(TAG, "data  still available");
    this->read_byte(&this->data_[this->data_index_]);
//    ESP_LOGD(TAG, "read %02x at index %i", this->data_[this->data_index_], this->data_index_);
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

float PM1006Component::get_setup_priority() const { return setup_priority::DATA; }

void PM1006Component::set_rx_mode_only(bool rx_mode_only) { /* this->rx_mode_only_ = rx_mode_only; */ }

#if 0
uint8_t PM1006Component::pm1006_checksum_(const uint8_t *command_data, uint8_t length) const {
// FIXME check if there is one
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}
#endif

optional<bool> PM1006Component::check_byte_() const {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  // index 0..2 are the fixed header
  if (index < sizeof(PM1006_RESPONSE_HEADER)) {
    return byte == PM1006_RESPONSE_HEADER[index];
  }

  // FIXME: until we figure out the checksum, there's nothing more we can check other than
  // 'did we receive all 16 bytes that come after the header'
  // just some additional notes here:
  // index 3..4 is unused
  // index 5..6 is our PM2.5 reading (3..6 is called DF1-DF4 in the datasheet at http://www.jdscompany.co.kr/download.asp?gubun=07&filename=PM1006_LED_PARTICLE_SENSOR_MODULE_SPECIFICATIONS.pdf
  // that datasheet goes on up to DF16, which is unused for PM1006 but used in PM1006K
  // so this code should be trivially extensible to support that one later
  if (index < (sizeof(PM1006_RESPONSE_HEADER)+16)) return true;

  return {};
}

void PM1006Component::parse_data_() {
  this->status_clear_warning();
  const float pm_2_5_concentration = this->get_16_bit_uint_(5);

  ESP_LOGD(TAG, "Got PM2.5 Concentration: %.1f µg/m³", pm_2_5_concentration);

  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  }
}

uint16_t PM1006Component::get_16_bit_uint_(uint8_t start_index) const {
  return (uint16_t(this->data_[start_index]) << 8) | uint16_t(this->data_[start_index+1]);
}

}  // namespace pm1006
}  // namespace esphome
