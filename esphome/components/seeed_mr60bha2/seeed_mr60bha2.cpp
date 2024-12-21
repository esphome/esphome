#include "seeed_mr60bha2.h"
#include "esphome/core/log.h"

#include <utility>

namespace esphome {
namespace seeed_mr60bha2 {

static const char *const TAG = "seeed_mr60bha2";

// Prints the component's configuration data. dump_config() prints all of the component's configuration
// items in an easy-to-read format, including the configuration key-value pairs.
void MR60BHA2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR60BHA2:");
#ifdef USE_SENSOR
  LOG_SENSOR(" ", "Breath Rate Sensor", this->breath_rate_sensor_);
  LOG_SENSOR(" ", "Heart Rate Sensor", this->heart_rate_sensor_);
  LOG_SENSOR(" ", "Distance Sensor", this->distance_sensor_);
#endif
}

// main loop
void MR60BHA2Component::loop() {
  uint8_t byte;

  // Is there data on the serial port
  while (this->available()) {
    this->read_byte(&byte);
    this->rx_message_.push_back(byte);
    if (!this->validate_message_()) {
      this->rx_message_.clear();
    }
  }
}

/**
 * @brief Calculate the checksum for a byte array.
 *
 * This function calculates the checksum for the provided byte array using an
 * XOR-based checksum algorithm.
 *
 * @param data The byte array to calculate the checksum for.
 * @param len The length of the byte array.
 * @return The calculated checksum.
 */
static uint8_t calculate_checksum(const uint8_t *data, size_t len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }
  checksum = ~checksum;
  return checksum;
}

/**
 * @brief Validate the checksum of a byte array.
 *
 * This function validates the checksum of the provided byte array by comparing
 * it to the expected checksum.
 *
 * @param data The byte array to validate.
 * @param len The length of the byte array.
 * @param expected_checksum The expected checksum.
 * @return True if the checksum is valid, false otherwise.
 */
static bool validate_checksum(const uint8_t *data, size_t len, uint8_t expected_checksum) {
  return calculate_checksum(data, len) == expected_checksum;
}

bool MR60BHA2Component::validate_message_() {
  size_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  if (at == 0) {
    return new_byte == FRAME_HEADER_BUFFER;
  }

  if (at <= 2) {
    return true;
  }
  uint16_t frame_id = encode_uint16(data[1], data[2]);

  if (at <= 4) {
    return true;
  }

  uint16_t length = encode_uint16(data[3], data[4]);

  if (at <= 6) {
    return true;
  }

  uint16_t frame_type = encode_uint16(data[5], data[6]);

  if (frame_type != BREATH_RATE_TYPE_BUFFER && frame_type != HEART_RATE_TYPE_BUFFER &&
      frame_type != DISTANCE_TYPE_BUFFER) {
    return false;
  }

  uint8_t header_checksum = new_byte;

  if (at == 7) {
    if (!validate_checksum(data, 7, header_checksum)) {
      ESP_LOGE(TAG, "HEAD_CKSUM_FRAME ERROR: 0x%02x", header_checksum);
      ESP_LOGV(TAG, "GET FRAME: %s", format_hex_pretty(data, 8).c_str());
      return false;
    }
    return true;
  }

  // Wait until all data is read
  if (at - 8 < length) {
    return true;
  }

  uint8_t data_checksum = new_byte;
  if (at == 8 + length) {
    if (!validate_checksum(data + 8, length, data_checksum)) {
      ESP_LOGE(TAG, "DATA_CKSUM_FRAME ERROR: 0x%02x", data_checksum);
      ESP_LOGV(TAG, "GET FRAME: %s", format_hex_pretty(data, 8 + length).c_str());
      return false;
    }
  }

  const uint8_t *frame_data = data + 8;
  ESP_LOGV(TAG, "Received Frame: ID: 0x%04x, Type: 0x%04x, Data: [%s] Raw Data: [%s]", frame_id, frame_type,
           format_hex_pretty(frame_data, length).c_str(), format_hex_pretty(this->rx_message_).c_str());
  this->process_frame_(frame_id, frame_type, data + 8, length);

  // Return false to reset rx buffer
  return false;
}

void MR60BHA2Component::process_frame_(uint16_t frame_id, uint16_t frame_type, const uint8_t *data, size_t length) {
  switch (frame_type) {
    case BREATH_RATE_TYPE_BUFFER:
      if (this->breath_rate_sensor_ != nullptr && length >= 4) {
        uint32_t current_breath_rate_int = encode_uint32(data[3], data[2], data[1], data[0]);
        if (current_breath_rate_int != 0) {
          float breath_rate_float;
          memcpy(&breath_rate_float, &current_breath_rate_int, sizeof(float));
          this->breath_rate_sensor_->publish_state(breath_rate_float);
        }
      }
      break;
    case HEART_RATE_TYPE_BUFFER:
      if (this->heart_rate_sensor_ != nullptr && length >= 4) {
        uint32_t current_heart_rate_int = encode_uint32(data[3], data[2], data[1], data[0]);
        if (current_heart_rate_int != 0) {
          float heart_rate_float;
          memcpy(&heart_rate_float, &current_heart_rate_int, sizeof(float));
          this->heart_rate_sensor_->publish_state(heart_rate_float);
        }
      }
      break;
    case DISTANCE_TYPE_BUFFER:
      if (!data[0]) {
        if (this->distance_sensor_ != nullptr && length >= 8) {
          uint32_t current_distance_int = encode_uint32(data[7], data[6], data[5], data[4]);
          float distance_float;
          memcpy(&distance_float, &current_distance_int, sizeof(float));
          this->distance_sensor_->publish_state(distance_float);
        }
      }
      break;
    default:
      break;
  }
}

}  // namespace seeed_mr60bha2
}  // namespace esphome
