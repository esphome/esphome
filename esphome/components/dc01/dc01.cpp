#include "dc01.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dc01 {

static const char *const TAG = "dc01";

static constexpr uint8_t DC01_RESPONSE_HEADER = 0xA5;
static constexpr uint8_t DCO1_RESPONSE_SIZE = 4;

void DC01Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DC01:");
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  this->check_uart_settings(9600);
}

void DC01Component::loop() {
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

// the checksum us the low 7 bits of the sum of the first 3 bytes of the payload
static inline uint8_t ESPHOME_ALWAYS_INLINE dc01_checksum(const uint8_t *command_data) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < 3; i++) {
    sum += command_data[i];
  }
  return sum & 0x7F;
}

optional<bool> DC01Component::check_byte_() const {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  // the first byte is the fixed header with value 0xA5
  if (index == 0) {
    return byte == DC01_RESPONSE_HEADER;
  }

  // just some additional notes here:
  // the second byte is the high byte of the pm2.5 concentration
  // the third byte is the low bytwe of the concentration
  if (index < 3)
    return true;

  // the fourth byte is the checksum
  if (index == 3) {
    uint8_t checksum = dc01_checksum(this->data_);
    if (checksum != byte) {
      ESP_LOGW(TAG, "Invalid checksum: %02x, expected %02x", checksum, byte);
      return false;
    }
    return {};
  }

  return false;
}

void DC01Component::parse_data_() {
  uint16_t data_h = static_cast<uint16_t>(this->data_[1]);
  uint16_t data_l = static_cast<uint16_t>(this->data_[2]);
  // pm2.5 is the high 7 bits plus the low 7 bits multiplied by a calibration factor of 0.4
  const int pm_2_5_concentration = static_cast<uint16_t>(((data_h << 7) + data_l) * 0.4);

  ESP_LOGD(TAG, "Got PM2.5 Concentration: %d µg/m³", pm_2_5_concentration);

  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  }
}

}  // namespace dc01
}  // namespace esphome
