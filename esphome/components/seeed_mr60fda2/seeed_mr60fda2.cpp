#include "seeed_mr60fda2.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <utility>

namespace esphome {
namespace seeed_mr60fda2 {

static const char *const TAG = "seeed_mr60fda2";

// Prints the component's configuration data. dump_config() prints all of the component's configuration
// items in an easy-to-read format, including the configuration key-value pairs.
void MR60FDA2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR60FDA2:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR(" ", "People Exist Binary Sensor", this->people_exist_binary_sensor_);
  LOG_BINARY_SENSOR(" ", "Is Fall Binary Sensor", this->fall_detected_binary_sensor_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON(" ", "Get Radar Parameters Button", this->get_radar_parameters_button_);
  LOG_BUTTON(" ", "Reset Radar Button", this->factory_reset_button_);
#endif
#ifdef USE_SELECT
  LOG_SELECT(" ", "Install Height Select", this->install_height_select_);
  LOG_SELECT(" ", "Height Threshold Select", this->height_threshold_select_);
  LOG_SELECT(" ", "Sensitivity Select", this->sensitivity_select_);
#endif
}

// Initialisation functions
void MR60FDA2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MR60FDA2...");
  this->check_uart_settings(115200);

  this->current_frame_locate_ = LOCATE_FRAME_HEADER;
  this->current_frame_id_ = 0;
  this->current_frame_len_ = 0;
  this->current_data_frame_len_ = 0;
  this->current_frame_type_ = 0;
  this->get_radar_parameters();

  memset(this->current_frame_buf_, 0, FRAME_BUF_MAX_SIZE);
  memset(this->current_data_buf_, 0, DATA_BUF_MAX_SIZE);

  ESP_LOGCONFIG(TAG, "Set up MR60FDA2 complete");
}

// main loop
void MR60FDA2Component::loop() {
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

static uint8_t find_nearest_index(float value, const float *arr, int size) {
  int nearest_index = 0;
  float min_diff = std::abs(value - arr[0]);
  for (int i = 1; i < size; ++i) {
    float diff = std::abs(value - arr[i]);
    if (diff < min_diff) {
      min_diff = diff;
      nearest_index = i;
    }
  }
  return nearest_index;
}

/**
 * @brief Convert a float value to a byte array.
 *
 * This function converts a float value to a byte array.
 *
 * @param value The float value to convert.
 * @param bytes The byte array to store the converted value.
 */
static void float_to_bytes(float value, unsigned char *bytes) {
  union {
    float float_value;
    unsigned char byte_array[4];
  } u;

  u.float_value = value;
  memcpy(bytes, u.byte_array, 4);
}

/**
 * @brief Convert a 32-bit unsigned integer to a byte array.
 *
 * This function converts a 32-bit unsigned integer to a byte array.
 *
 * @param value The 32-bit unsigned integer to convert.
 * @param bytes The byte array to store the converted value.
 */
static void int_to_bytes(uint32_t value, unsigned char *bytes) {
  bytes[0] = value & 0xFF;
  bytes[1] = (value >> 8) & 0xFF;
  bytes[2] = (value >> 16) & 0xFF;
  bytes[3] = (value >> 24) & 0xFF;
}

void MR60FDA2Component::split_frame_(uint8_t buffer) {
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
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_LENGTH_FRAME_L:
      this->current_data_frame_len_ += buffer;
      if (this->current_data_frame_len_ > DATA_BUF_MAX_SIZE) {
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
      break;
    case LOCATE_TYPE_FRAME2:
      this->current_frame_type_ += buffer;
      if ((this->current_frame_type_ == IS_FALL_TYPE_BUFFER) ||
          (this->current_frame_type_ == PEOPLE_EXIST_TYPE_BUFFER) ||
          (this->current_frame_type_ == RESULT_INSTALL_HEIGHT) || (this->current_frame_type_ == RESULT_PARAMETERS) ||
          (this->current_frame_type_ == RESULT_HEIGHT_THRESHOLD) || (this->current_frame_type_ == RESULT_SENSITIVITY)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      } else {
        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    case LOCATE_HEAD_CKSUM_FRAME:
      if (validate_checksum(this->current_frame_buf_, this->current_frame_len_, buffer)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
      } else {
        ESP_LOGD(TAG, "HEAD_CKSUM_FRAME ERROR: 0x%02x", buffer);
        ESP_LOGV(TAG, "CURRENT_FRAME: %s %s",
                 format_hex_pretty(this->current_frame_buf_, this->current_frame_len_).c_str(),
                 format_hex_pretty(&buffer, 1).c_str());
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
      if (validate_checksum(this->current_data_buf_, this->current_data_frame_len_, buffer)) {
        this->current_frame_len_++;
        this->current_frame_buf_[this->current_frame_len_ - 1] = buffer;
        this->current_frame_locate_++;
        this->process_frame_();
      } else {
        ESP_LOGD(TAG, "DATA_CKSUM_FRAME ERROR: 0x%02x", buffer);
        ESP_LOGV(TAG, "GET CURRENT_FRAME: %s %s",
                 format_hex_pretty(this->current_frame_buf_, this->current_frame_len_).c_str(),
                 format_hex_pretty(&buffer, 1).c_str());

        this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      }
      break;
    default:
      break;
  }
}

void MR60FDA2Component::process_frame_() {
  switch (this->current_frame_type_) {
    case IS_FALL_TYPE_BUFFER:
      if (this->fall_detected_binary_sensor_ != nullptr) {
        this->fall_detected_binary_sensor_->publish_state(this->current_frame_buf_[LEN_TO_HEAD_CKSUM]);
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;

    case PEOPLE_EXIST_TYPE_BUFFER:
      if (this->people_exist_binary_sensor_ != nullptr)
        this->people_exist_binary_sensor_->publish_state(this->current_frame_buf_[LEN_TO_HEAD_CKSUM]);
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;

    case RESULT_INSTALL_HEIGHT:
      if (this->current_data_buf_[0]) {
        ESP_LOGD(TAG, "Successfully set the mounting height");
      } else {
        ESP_LOGD(TAG, "Failed to set the mounting height");
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;

    case RESULT_HEIGHT_THRESHOLD:
      if (this->current_data_buf_[0]) {
        ESP_LOGD(TAG, "Successfully set the height threshold");
      } else {
        ESP_LOGD(TAG, "Failed to set the height threshold");
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;

    case RESULT_SENSITIVITY:
      if (this->current_data_buf_[0]) {
        ESP_LOGD(TAG, "Successfully set the sensitivity");
      } else {
        ESP_LOGD(TAG, "Failed to set the sensitivity");
      }
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;

    case RESULT_PARAMETERS: {
      float install_height_float = 0;
      float height_threshold_float = 0;
      uint32_t current_sensitivity = 0;
      if (this->install_height_select_ != nullptr) {
        uint32_t current_install_height_int =
            encode_uint32(current_data_buf_[3], current_data_buf_[2], current_data_buf_[1], current_data_buf_[0]);

        install_height_float = bit_cast<float>(current_install_height_int);
        uint32_t select_index = find_nearest_index(install_height_float, INSTALL_HEIGHT, 7);
        this->install_height_select_->publish_state(this->install_height_select_->at(select_index).value());
      }

      if (this->height_threshold_select_ != nullptr) {
        uint32_t current_height_threshold_int =
            encode_uint32(current_data_buf_[7], current_data_buf_[6], current_data_buf_[5], current_data_buf_[4]);

        height_threshold_float = bit_cast<float>(current_height_threshold_int);
        size_t select_index = find_nearest_index(height_threshold_float, HEIGHT_THRESHOLD, 7);
        this->height_threshold_select_->publish_state(this->height_threshold_select_->at(select_index).value());
      }

      if (this->sensitivity_select_ != nullptr) {
        current_sensitivity =
            encode_uint32(current_data_buf_[11], current_data_buf_[10], current_data_buf_[9], current_data_buf_[8]);

        uint32_t select_index = find_nearest_index(current_sensitivity, SENSITIVITY, 3);
        this->sensitivity_select_->publish_state(this->sensitivity_select_->at(select_index).value());
      }

      ESP_LOGD(TAG, "Mounting height: %.2f, Height threshold: %.2f, Sensitivity: %" PRIu32, install_height_float,
               height_threshold_float, current_sensitivity);
      this->current_frame_locate_ = LOCATE_FRAME_HEADER;
      break;
    }
    default:
      break;
  }
}

// Send Heartbeat Packet Command
void MR60FDA2Component::set_install_height(uint8_t index) {
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x04, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00};
  float_to_bytes(INSTALL_HEIGHT[index], &send_data[8]);
  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV(TAG, "SEND INSTALL HEIGHT FRAME: %s", format_hex_pretty(send_data, 13).c_str());
}

void MR60FDA2Component::set_height_threshold(uint8_t index) {
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x08, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00};
  float_to_bytes(INSTALL_HEIGHT[index], &send_data[8]);
  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV(TAG, "SEND HEIGHT THRESHOLD: %s", format_hex_pretty(send_data, 13).c_str());
}

void MR60FDA2Component::set_sensitivity(uint8_t index) {
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x0A, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00};

  int_to_bytes(SENSITIVITY[index], &send_data[8]);

  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV(TAG, "SEND SET SENSITIVITY: %s", format_hex_pretty(send_data, 13).c_str());
}

void MR60FDA2Component::get_radar_parameters() {
  uint8_t send_data[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x06, 0xF6};
  this->write_array(send_data, 8);
  ESP_LOGV(TAG, "SEND GET PARAMETERS: %s", format_hex_pretty(send_data, 8).c_str());
}

void MR60FDA2Component::factory_reset() {
  uint8_t send_data[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x21, 0x10, 0xCF};
  this->write_array(send_data, 8);
  ESP_LOGV(TAG, "SEND RESET: %s", format_hex_pretty(send_data, 8).c_str());
  this->get_radar_parameters();
}

}  // namespace seeed_mr60fda2
}  // namespace esphome
