#include "wts01.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace wts01 {

constexpr uint8_t HEADER_1 = 0x55;
constexpr uint8_t HEADER_2 = 0x01;
constexpr uint8_t HEADER_3 = 0x01;
constexpr uint8_t HEADER_4 = 0x04;

static const char *const TAG = "wts01";

void WTS01Sensor::loop() {
  // Process all available data at once
  while (this->available()) {
    uint8_t c;
    if (this->read_byte(&c)) {
      this->handle_char_(c);
    }
  }
}

void WTS01Sensor::dump_config() { LOG_SENSOR("", "WTS01 Sensor", this); }

void WTS01Sensor::handle_char_(uint8_t c) {
  // State machine for processing the header. Reset if something doesn't match.
  if (this->buffer_pos_ == 0 && c != HEADER_1) {
    return;
  }

  if (this->buffer_pos_ == 1 && c != HEADER_2) {
    this->buffer_pos_ = 0;
    return;
  }

  if (this->buffer_pos_ == 2 && c != HEADER_3) {
    this->buffer_pos_ = 0;
    return;
  }

  if (this->buffer_pos_ == 3 && c != HEADER_4) {
    this->buffer_pos_ = 0;
    return;
  }

  // Add byte to buffer
  this->buffer_[this->buffer_pos_++] = c;

  // Process complete packet
  if (this->buffer_pos_ >= PACKET_SIZE) {
    this->process_packet_();
    this->buffer_pos_ = 0;
  }
}

void WTS01Sensor::process_packet_() {
  // Based on Tasmota implementation
  // Format: 55 01 01 04 01 11 16 12 95
  // header            T  Td Ck  - T = Temperature, Td = Temperature decimal, Ck = Checksum
  uint8_t calculated_checksum = 0;
  for (uint8_t i = 0; i < PACKET_SIZE - 1; i++) {
    calculated_checksum += this->buffer_[i];
  }

  uint8_t received_checksum = this->buffer_[PACKET_SIZE - 1];
  if (calculated_checksum != received_checksum) {
    ESP_LOGW(TAG, "WTS01 Checksum doesn't match: 0x%02X != 0x%02X", received_checksum, calculated_checksum);
    return;
  }

  // Extract temperature value
  int8_t temp = this->buffer_[6];
  int32_t sign = 1;

  // Handle negative temperatures
  if (temp < 0) {
    sign = -1;
  }

  // Calculate temperature (temp + decimal/100)
  float temperature = static_cast<float>(temp) + (sign * static_cast<float>(this->buffer_[7]) / 100.0f);

  ESP_LOGV(TAG, "Received new temperature: %.2f°C", temperature);

  this->publish_state(temperature);
}

}  // namespace wts01
}  // namespace esphome
