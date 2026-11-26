#include "seeed_mr60bha2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <utility>

namespace esphome::seeed_mr60bha2 {

ESPHOME_LOG_TAG(TAG, "seeed_mr60bha2");

// Maximum bytes to log in verbose hex output
static constexpr size_t MR60BHA2_MAX_LOG_BYTES = 64;

// Prints the component's configuration data. dump_config() prints all of the component's configuration
// items in an easy-to-read format, including the configuration key-value pairs.
void MR60BHA2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR60BHA2:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR(" ", "People Exist Binary Sensor", this->has_target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR(" ", "Breath Rate Sensor", this->breath_rate_sensor_);
  LOG_SENSOR(" ", "Heart Rate Sensor", this->heart_rate_sensor_);
  LOG_SENSOR(" ", "Distance Sensor", this->distance_sensor_);
  LOG_SENSOR(" ", "Target Number Sensor", this->num_targets_sensor_);
#endif
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
  return static_cast<uint8_t>(~xor_bytes(data, len)) == expected_checksum;
}

static bool is_known_type(uint16_t type) {
  return type == BREATH_RATE_TYPE_BUFFER || type == HEART_RATE_TYPE_BUFFER || type == DISTANCE_TYPE_BUFFER ||
         type == PEOPLE_EXIST_TYPE_BUFFER || type == PRINT_CLOUD_BUFFER;
}

// An unknown type or a bad header checksum fails, so the header is dropped at once
bool header_valid(const uint8_t *frame) {
  if (!is_known_type(encode_uint16(frame[5], frame[6])))
    return false;
  constexpr size_t cksum_pos = PayloadLength::PAYLOAD_AT - 1;
  if (!validate_checksum(frame, cksum_pos, frame[cksum_pos])) {
    ESP_LOGE(TAG, "HEAD_CKSUM_FRAME ERROR: 0x%02x", frame[cksum_pos]);
    ESP_LOGV_HEX(TAG, frame, PayloadLength::PAYLOAD_AT, MR60BHA2_MAX_LOG_BYTES, "GET FRAME: %s");
    return false;
  }
  return true;
}

void warn_too_long(size_t len) {
  ESP_LOGW(TAG, "Skipped a %u byte frame, longer than the %u byte buffer", static_cast<unsigned>(len),
           static_cast<unsigned>(FRAME_MAX_SIZE));
}

// The reader checks the type and the header; the data checksum is checked here
static bool frame_valid(const uint8_t *frame, size_t len) {
  if (!validate_checksum(frame + PayloadLength::PAYLOAD_AT, len - FRAME_OVERHEAD, frame[len - 1])) {
    ESP_LOGE(TAG, "DATA_CKSUM_FRAME ERROR: 0x%02x", frame[len - 1]);
    ESP_LOGV_HEX(TAG, frame, len - 1, MR60BHA2_MAX_LOG_BYTES, "GET FRAME: %s");
    return false;
  }
  return true;
}

void MR60BHA2Component::loop() {
  auto handler = [this](const uint8_t *frame, size_t len) {
    if (!frame_valid(frame, len))
      return false;
    this->handle_frame_(frame, len);
    return true;
  };
  this->read_frames(this->reader_, handler, App.get_loop_component_start_time(), this->last_rx_);
}

void MR60BHA2Component::handle_frame_(const uint8_t *frame, size_t len) {
  const uint16_t frame_id = encode_uint16(frame[1], frame[2]);
  const uint16_t frame_type = encode_uint16(frame[5], frame[6]);
  const uint8_t *frame_data = frame + PayloadLength::PAYLOAD_AT;
  const size_t length = len - FRAME_OVERHEAD;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf1[format_hex_pretty_size(MR60BHA2_MAX_LOG_BYTES)];
  char hex_buf2[format_hex_pretty_size(MR60BHA2_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Received Frame: ID: 0x%04x, Type: 0x%04x, Data: [%s] Raw Data: [%s]", frame_id, frame_type,
           format_hex_pretty_to(hex_buf1, sizeof(hex_buf1), frame_data, length),
           format_hex_pretty_to(hex_buf2, sizeof(hex_buf2), frame, len));
  this->process_frame_(frame_id, frame_type, frame_data, length);
}

void MR60BHA2Component::process_frame_(uint16_t frame_id, uint16_t frame_type, const uint8_t *data, size_t length) {
  switch (frame_type) {
    case BREATH_RATE_TYPE_BUFFER:
#ifdef USE_SENSOR
      if (this->breath_rate_sensor_ != nullptr && length >= 4) {
        uint32_t current_breath_rate_int = encode_uint32(data[3], data[2], data[1], data[0]);
        if (current_breath_rate_int != 0) {
          float breath_rate_float;
          memcpy(&breath_rate_float, &current_breath_rate_int, sizeof(float));
          this->breath_rate_sensor_->publish_state(breath_rate_float);
        }
      }
#endif
      break;
    case PEOPLE_EXIST_TYPE_BUFFER:
#ifdef USE_BINARY_SENSOR
      if (this->has_target_binary_sensor_ != nullptr && length >= 2) {
        uint16_t has_target_int = encode_uint16(data[1], data[0]);
        this->has_target_binary_sensor_->publish_state(has_target_int);
#ifdef USE_SENSOR
        if (has_target_int == 0) {
          PUBLISH_STATE(this->breath_rate_sensor_, 0.0);
          PUBLISH_STATE(this->heart_rate_sensor_, 0.0);
          PUBLISH_STATE(this->distance_sensor_, 0.0);
          PUBLISH_STATE(this->num_targets_sensor_, 0);
        }
#endif
      }
#endif
      break;
    case HEART_RATE_TYPE_BUFFER:
#ifdef USE_SENSOR
      if (this->heart_rate_sensor_ != nullptr && length >= 4) {
        uint32_t current_heart_rate_int = encode_uint32(data[3], data[2], data[1], data[0]);
        if (current_heart_rate_int != 0) {
          float heart_rate_float;
          memcpy(&heart_rate_float, &current_heart_rate_int, sizeof(float));
          this->heart_rate_sensor_->publish_state(heart_rate_float);
        }
      }
#endif
      break;
    case DISTANCE_TYPE_BUFFER:
#ifdef USE_SENSOR
      if (length >= 1 && data[0] != 0) {
        if (this->distance_sensor_ != nullptr && length >= 8) {
          uint32_t current_distance_int = encode_uint32(data[7], data[6], data[5], data[4]);
          float distance_float;
          memcpy(&distance_float, &current_distance_int, sizeof(float));
          this->distance_sensor_->publish_state(distance_float);
        }
      }
#endif
      break;
    case PRINT_CLOUD_BUFFER:
#ifdef USE_SENSOR
      if (this->num_targets_sensor_ != nullptr && length >= 4) {
        uint32_t current_num_targets_int = encode_uint32(data[3], data[2], data[1], data[0]);
        this->num_targets_sensor_->publish_state(current_num_targets_int);
      }
#endif
      break;
    default:
      break;
  }
}

}  // namespace esphome::seeed_mr60bha2
