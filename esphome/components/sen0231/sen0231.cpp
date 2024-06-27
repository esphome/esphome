#include "sen0231.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sen0231_sensor {

static const char *const TAG = "sen0231_sensor.sensor";

static constexpr uint8_t START_BYTE = 0;
static constexpr uint8_t NAME_BYTE = 1;
static constexpr uint8_t UNIT_BYTE = 2;
static constexpr uint8_t NUM_DECIMAL_BYTE = 3;
static constexpr uint8_t CONCENTRATION_HIGH_BYTE = 4;
static constexpr uint8_t CONCENTRATION_LOW_BYTE = 5;
static constexpr uint8_t FULL_RANGE_HIGH_BYTE = 6;
static constexpr uint8_t FULL_RANGE_LOW_BYTE = 7;
static constexpr uint8_t CHECKSUM_BYTE = 8;
static constexpr uint8_t NUM_BYTES = 9;

void Sen0231Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up sen0231..."); }

void Sen0231Sensor::update() { this->read_data_(); }

void Sen0231Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DF Robot Formaldehyde Sensor sen0231:");
  LOG_UPDATE_INTERVAL(this);
}

uint8_t check_sum(const uint8_t array[], uint8_t length) {
  uint8_t sum = 0;
  for (int i = 1; i < length - 1; i++) {
    sum += array[i];
  }
  sum = (~sum) + 1;
  return sum;
}

void Sen0231Sensor::read_data_() {
  uint8_t buffer[NUM_BYTES] = {0};

  // Read the full buffer, but we are only interested in the last frame
  while (available() > 0) {
    for (uint8_t i = 0; i < NUM_BYTES - 1; i++) {
      // Shift buffer
      buffer[i] = buffer[i + 1];
    }

    // Fill into last element
    buffer[NUM_BYTES - 1] = read();
  }

  uint8_t check_val = check_sum(buffer, NUM_BYTES);
  if ((buffer[START_BYTE] != 0xFF) || (buffer[NAME_BYTE] != 0x17) || (buffer[UNIT_BYTE] != 0x04) ||
      (buffer[CHECKSUM_BYTE] != check_val)) {
    ESP_LOGE(TAG, "Error in received data %d, %d, %d, %d", buffer[START_BYTE], buffer[NAME_BYTE], buffer[UNIT_BYTE],
             buffer[CHECKSUM_BYTE]);
    return;
  }

  float val = ((uint16_t) (buffer[CONCENTRATION_HIGH_BYTE] << 8) + buffer[CONCENTRATION_LOW_BYTE]) / 1000.0;
  this->publish_state(val);
}

}  // namespace sen0231_sensor
}  // namespace esphome
