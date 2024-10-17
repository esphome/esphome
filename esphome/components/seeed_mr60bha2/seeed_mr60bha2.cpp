#include "esphome/core/log.h"
#include "seeed_mr60bha2.h"

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

// Initialisation functions
void MR60BHA2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MR60BHA2...");
  this->check_uart_settings(115200);

  this->current_frame_locate_ = LOCATE_FRAME_HEADER;
  this->current_frame_id_ = 0;
  this->current_frame_len_ = 0;
  this->current_data_frame_len_ = 0;
  this->current_frame_type_ = 0;
  this->current_breath_rate_int_ = 0;
  this->current_heart_rate_int_ = 0;
  this->current_distance_int_ = 0;

  memset(this->current_frame_buf_, 0, FRAME_BUF_MAX_SIZE);
  memset(this->current_data_buf_, 0, DATA_BUF_MAX_SIZE);

  ESP_LOGCONFIG(TAG, "Set up MR60BHA2 complete");
}

// main loop
void MR60BHA2Component::loop() {
  uint8_t byte;

  // Is there data on the serial port
  while (this->available()) {
    this->read_byte(&byte);
    this->split_frame_(byte);  // split data frame
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
uint8_t MR60BHA2Component::calculate_checksum_(const uint8_t *data, size_t len) {
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
bool MR60BHA2Component::validate_checksum_(const uint8_t *data, size_t len, uint8_t expected_checksum) {
  return calculate_checksum_(data, len) == expected_checksum;
}

void MR60BHA2Component::split_frame_(uint8_t buffer) {
  switch (this->current_frame_locate_) {
    case LOCATE_FRAME_HEADER:  // starting buffer
      if (buffer == FRAME_HEADER_BUFFER) {
        this->current_frame_len_ = 1;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      }
      break;
    case LOCATE_ID_FRAME1:
      this->current_frame_id_ = buffer << 8;
      this->current_frame_len_++;
      this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
      this->current_frame_locate_++;
      break;
    case LOCATE_ID_FRAME2:
      this->current_frame_id_ += buffer;
      this->current_frame_len_++;
      this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
      this->current_frame_locate_++;
      break;
    case LOCATE_LENGTH_FRAME_H:
      this->current_data_frame_len_ = buffer << 8;
      if (this->current_data_frame_len_ == 0x00) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      } else {
        // ESP_LOGD(TAG, "DATA_FRAME_LEN_H: 0x%02x", buffer);
        // ESP_LOGD(TAG, "CURRENT_FRAME_LEN_H: 0x%04x", this->current_data_frame_len_);
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_LENGTH_FRAME_L:
      this->current_data_frame_len_ += buffer;
      if (this->current_data_frame_len_ > DATA_BUF_MAX_SIZE) {
        // ESP_LOGD(TAG, "DATA_FRAME_LEN_L: 0x%02x", buffer);
        // ESP_LOGD(TAG, "CURRENT_FRAME_LEN: 0x%04x", this->current_data_frame_len_);
        // ESP_LOGD(TAG, "DATA_FRAME_LEN ERROR: %d", this->current_data_frame_len_);
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      } else {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      }
      break;
    case LOCATE_TYPE_FRAME1:
      this->current_frame_type_ = buffer << 8;
      this->current_frame_len_++;
      this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
      this->current_frame_locate_++;
      // ESP_LOGD(TAG, "GET LOCATE_TYPE_FRAME1: 0x%02x", this->current_frame_buf_[this->current_frame_len_ - 1]);
      break;
    case LOCATE_TYPE_FRAME2:
      this->current_frame_type_ += buffer;
      if ((this->current_frame_type_ == BREATH_RATE_TYPE_BUFFER) ||
          (this->current_frame_type_ == HEART_RATE_TYPE_BUFFER) ||
          (this->current_frame_type_ == DISTANCE_TYPE_BUFFER)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
        // ESP_LOGD(TAG, "GET CURRENT_FRAME_TYPE: 0x%02x 0x%02x", this->current_frame_buf_[this->current_frame_len_ -
        // 2],
        //          this->current_frame_buf_[this->current_frame_len_ - 1]);
      } else {
        // ESP_LOGD(TAG, "CURRENT_FRAME_TYPE NOT FOUND: 0x%02x 0x%02x",
        //          this->current_frame_buf_[this->current_frame_len_ - 2],
        //          this->current_frame_buf_[this->current_frame_len_ - 1]);
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_HEAD_CKSUM_FRAME:
      if (this->validate_checksum_(this->current_frame_buf_, this->current_frame_len_, buffer)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      } else {
        ESP_LOGD(TAG, "HEAD_CKSUM_FRAME ERROR: 0x%02x", buffer);
        ESP_LOGD(TAG, "GET CURRENT_FRAME:");
        for (size_t i = 0; i < this->current_frame_len_; i++) {
          ESP_LOGD(TAG, "  0x%02x", current_frame_buf_[i]);
        }
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_DATA_FRAME:
      this->current_frame_len_++;
      this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
      this->current_data_buf_[this->current_frame_len_ - LEN_TO_DATA_FRAME] = buffer;
      if (this->current_frame_len_ - LEN_TO_HEAD_CKSUM == this->current_data_frame_len_) {
        this->current_frame_locate_++;
      }
      if (this->current_frame_len_ > FRAME_BUF_MAX_SIZE) {
        ESP_LOGD(TAG, "PRACTICE_DATA_FRAME_LEN ERROR: %d", this->current_frame_len_ - LEN_TO_HEAD_CKSUM);
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_DATA_CKSUM_FRAME:
      if (this->validate_checksum_(this->current_data_buf_, this->current_data_frame_len_, buffer)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
        this->process_frame_();
      } else {
        ESP_LOGD(TAG, "DATA_CKSUM_FRAME ERROR: 0x%02x", buffer);
        ESP_LOGD(TAG, "GET CURRENT_FRAME:");
        for (size_t i = 0; i < this->current_frame_len_; i++) {
          ESP_LOGD(TAG, "  0x%02x", current_frame_buf_[i]);
        }
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    default:
      break;
  }
}

void MR60BHA2Component::process_frame_() {
  switch (this->current_frame_type_) {
    case BREATH_RATE_TYPE_BUFFER:
      if (this->breath_rate_sensor_ != nullptr) {
        this->current_breath_rate_int_ =
            (static_cast<uint32_t>(current_data_buf_[3]) << 24) | (static_cast<uint32_t>(current_data_buf_[2]) << 16) |
            (static_cast<uint32_t>(current_data_buf_[1]) << 8) | static_cast<uint32_t>(current_data_buf_[0]);
        float breath_rate_float;
        memcpy(&breath_rate_float, &current_breath_rate_int_, sizeof(float));
        this->breath_rate_sensor_->publish_state(breath_rate_float);
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;
    case HEART_RATE_TYPE_BUFFER:
      if (this->heart_rate_sensor_ != nullptr) {
        this->current_heart_rate_int_ =
            (static_cast<uint32_t>(current_data_buf_[3]) << 24) | (static_cast<uint32_t>(current_data_buf_[2]) << 16) |
            (static_cast<uint32_t>(current_data_buf_[1]) << 8) | static_cast<uint32_t>(current_data_buf_[0]);
        float heart_rate_float;
        memcpy(&heart_rate_float, &current_heart_rate_int_, sizeof(float));
        this->heart_rate_sensor_->publish_state(heart_rate_float);
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;
    case DISTANCE_TYPE_BUFFER:
      if (!current_data_buf_[0]) {
        // ESP_LOGD(TAG, "Successfully set the mounting height");
        if (this->distance_sensor_ != nullptr) {
          this->current_distance_int_ = (static_cast<uint32_t>(current_data_buf_[7]) << 24) |
                                        (static_cast<uint32_t>(current_data_buf_[6]) << 16) |
                                        (static_cast<uint32_t>(current_data_buf_[5]) << 8) |
                                        static_cast<uint32_t>(current_data_buf_[4]);
          float distance_float;
          memcpy(&distance_float, &current_distance_int_, sizeof(float));
          this->distance_sensor_->publish_state(distance_float);
        }
      } else
        ESP_LOGD(TAG, "Distance information is not output");
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;
    default:
      break;
  }
}

}  // namespace seeed_mr60bha2
}  // namespace esphome
