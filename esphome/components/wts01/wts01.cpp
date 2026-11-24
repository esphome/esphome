#include "wts01.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::wts01 {

ESPHOME_LOG_TAG(TAG, "wts01");

void WTS01Sensor::loop() {
  this->read_available([this](uint8_t c) {
    if (const uint8_t *packet = this->reader_.feed(c)) {
      this->process_packet_(packet);
    }
  });
}

void WTS01Sensor::dump_config() { LOG_SENSOR("", "WTS01 Sensor", this); }

void WTS01Sensor::process_packet_(const uint8_t *packet) {
  // Based on Tasmota implementation
  // Format: 55 01 01 04 01 11 16 12 95
  // header            T  Td Ck  - T = Temperature, Td = Temperature decimal, Ck = Checksum
  const uint8_t calculated_checksum = sum_bytes(packet, PACKET_SIZE - 1);

  uint8_t received_checksum = packet[PACKET_SIZE - 1];
  if (calculated_checksum != received_checksum) {
    ESP_LOGW(TAG, "WTS01 Checksum doesn't match: 0x%02X != 0x%02X", received_checksum, calculated_checksum);
    return;
  }

  // Extract temperature value
  const uint8_t raw = packet[6];

  // WTS01 encodes sign in bit 7, magnitude in bits 0-6
  const bool negative = (raw & 0x80) != 0;
  const uint8_t magnitude = raw & 0x7F;

  const float decimal = static_cast<float>(packet[7]) / 100.0f;

  float temperature = static_cast<float>(magnitude) + decimal;

  if (negative) {
    temperature = -temperature;
  }

  ESP_LOGV(TAG, "Received new temperature: %.2f°C", temperature);

  this->publish_state(temperature);
}

}  // namespace esphome::wts01
