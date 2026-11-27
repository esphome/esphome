#include "seeed_mr60fda2.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <utility>

namespace esphome::seeed_mr60fda2 {

ESPHOME_LOG_TAG(TAG, "seeed_mr60fda2");

// Maximum bytes to log in verbose hex output
static constexpr size_t MR60FDA2_MAX_LOG_BYTES = 64;

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
void MR60FDA2Component::setup() { this->get_radar_parameters(); }

// The checksum is the inverted XOR of the bytes
static uint8_t calculate_checksum(const uint8_t *data, size_t len) { return ~xor_bytes(data, len); }

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

static bool is_known_type(uint16_t type) {
  return type == IS_FALL_TYPE_BUFFER || type == PEOPLE_EXIST_TYPE_BUFFER || type == RESULT_INSTALL_HEIGHT ||
         type == RESULT_PARAMETERS || type == RESULT_HEIGHT_THRESHOLD || type == RESULT_SENSITIVITY;
}

// An unknown type or a bad header checksum fails, so the header is dropped at once
bool header_valid(const uint8_t *frame) {
  const uint16_t type = encode_uint16(frame[5], frame[6]);
  // A parameter reply is read for 12 data bytes
  if (!is_known_type(type) || (type == RESULT_PARAMETERS && encode_uint16(frame[3], frame[4]) < 12))
    return false;
  if (!validate_checksum(frame, LEN_TO_HEAD_CKSUM - 1, frame[LEN_TO_HEAD_CKSUM - 1])) {
    ESP_LOGD(TAG, "HEAD_CKSUM_FRAME ERROR: 0x%02x", frame[LEN_TO_HEAD_CKSUM - 1]);
    ESP_LOGV_HEX(TAG, frame, LEN_TO_HEAD_CKSUM, MR60FDA2_MAX_LOG_BYTES, "CURRENT_FRAME: %s");
    return false;
  }
  return true;
}

static bool frame_valid(const uint8_t *frame, size_t len) {
  if (!validate_checksum(frame + LEN_TO_HEAD_CKSUM, len - LEN_TO_DATA_FRAME, frame[len - 1])) {
    ESP_LOGD(TAG, "DATA_CKSUM_FRAME ERROR: 0x%02x", frame[len - 1]);
    ESP_LOGV_HEX(TAG, frame, len, MR60FDA2_MAX_LOG_BYTES, "GET CURRENT_FRAME: %s");
    return false;
  }
  return true;
}

void MR60FDA2Component::loop() {
  auto handler = [this](const uint8_t *frame, size_t len) {
    if (!frame_valid(frame, len))
      return false;
    this->process_frame_(frame);
    return true;
  };
  this->read_frames(this->reader_, handler, App.get_loop_component_start_time(), this->last_rx_);
}

#ifdef USE_SELECT
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
#endif

// Read with 32-bit loads or progmem_memcpy only; the radar takes the floats little-endian
alignas(4) static const float INSTALL_HEIGHT[7] PROGMEM = {2.4f, 2.5f, 2.6f, 2.7f, 2.8f, 2.9f, 3.0f};
alignas(4) static const float HEIGHT_THRESHOLD[7] PROGMEM = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
alignas(4) static const float SENSITIVITY[3] PROGMEM = {3, 15, 30};

void MR60FDA2Component::process_frame_(const uint8_t *frame) {
  const uint8_t *data = frame + LEN_TO_HEAD_CKSUM;
  switch (encode_uint16(frame[5], frame[6])) {
    case IS_FALL_TYPE_BUFFER:
#ifdef USE_BINARY_SENSOR
      PUBLISH_STATE(this->fall_detected_binary_sensor_, data[0]);
#endif
      break;

    case PEOPLE_EXIST_TYPE_BUFFER:
#ifdef USE_BINARY_SENSOR
      PUBLISH_STATE(this->people_exist_binary_sensor_, data[0]);
#endif
      break;

    case RESULT_INSTALL_HEIGHT:
      if (data[0]) {
        ESP_LOGD(TAG, "Successfully set the mounting height");
      } else {
        ESP_LOGD(TAG, "Failed to set the mounting height");
      }
      break;

    case RESULT_HEIGHT_THRESHOLD:
      if (data[0]) {
        ESP_LOGD(TAG, "Successfully set the height threshold");
      } else {
        ESP_LOGD(TAG, "Failed to set the height threshold");
      }
      break;

    case RESULT_SENSITIVITY:
      if (data[0]) {
        ESP_LOGD(TAG, "Successfully set the sensitivity");
      } else {
        ESP_LOGD(TAG, "Failed to set the sensitivity");
      }
      break;

    case RESULT_PARAMETERS: {
      float install_height_float = 0;
      float height_threshold_float = 0;
      uint32_t current_sensitivity = 0;
#ifdef USE_SELECT
      if (this->install_height_select_ != nullptr) {
        uint32_t current_install_height_int = encode_uint32(data[3], data[2], data[1], data[0]);

        install_height_float = bit_cast<float>(current_install_height_int);
        uint32_t select_index = find_nearest_index(install_height_float, INSTALL_HEIGHT, 7);
        this->install_height_select_->publish_state(select_index);
      }

      if (this->height_threshold_select_ != nullptr) {
        uint32_t current_height_threshold_int = encode_uint32(data[7], data[6], data[5], data[4]);

        height_threshold_float = bit_cast<float>(current_height_threshold_int);
        size_t select_index = find_nearest_index(height_threshold_float, HEIGHT_THRESHOLD, 7);
        this->height_threshold_select_->publish_state(select_index);
      }

      if (this->sensitivity_select_ != nullptr) {
        current_sensitivity = encode_uint32(data[11], data[10], data[9], data[8]);

        uint32_t select_index = find_nearest_index(current_sensitivity, SENSITIVITY, 3);
        this->sensitivity_select_->publish_state(select_index);
      }
#endif

      ESP_LOGD(TAG, "Mounting height: %.2f, Height threshold: %.2f, Sensitivity: %" PRIu32, install_height_float,
               height_threshold_float, current_sensitivity);
      break;
    }
    default:
      break;
  }
}

// Send Heartbeat Packet Command
void MR60FDA2Component::set_install_height(uint8_t index) {
  if (index >= std::size(INSTALL_HEIGHT))
    return;
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x04, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00};
  progmem_memcpy(&send_data[8], &INSTALL_HEIGHT[index], 4);
  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV_HEX(TAG, send_data, 13, 13, "SEND INSTALL HEIGHT FRAME: %s");
}

void MR60FDA2Component::set_height_threshold(uint8_t index) {
  if (index >= std::size(HEIGHT_THRESHOLD))
    return;
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x08, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00};
  progmem_memcpy(&send_data[8], &HEIGHT_THRESHOLD[index], 4);
  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV_HEX(TAG, send_data, 13, 13, "SEND HEIGHT THRESHOLD: %s");
}

void MR60FDA2Component::set_sensitivity(uint8_t index) {
  if (index >= std::size(SENSITIVITY))
    return;
  uint8_t send_data[13] = {0x01, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x0A, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00};

  put_le32(send_data + 8, static_cast<uint32_t>(SENSITIVITY[index]));

  send_data[12] = calculate_checksum(send_data + 8, 4);
  this->write_array(send_data, 13);
  ESP_LOGV_HEX(TAG, send_data, 13, 13, "SEND SET SENSITIVITY: %s");
}

void MR60FDA2Component::get_radar_parameters() {
  uint8_t send_data[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x06, 0xF6};
  this->write_array(send_data, 8);
  ESP_LOGV_HEX(TAG, send_data, 8, 8, "SEND GET PARAMETERS: %s");
}

void MR60FDA2Component::factory_reset() {
  uint8_t send_data[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x21, 0x10, 0xCF};
  this->write_array(send_data, 8);
  ESP_LOGV_HEX(TAG, send_data, 8, 8, "SEND RESET: %s");
  this->get_radar_parameters();
}

}  // namespace esphome::seeed_mr60fda2
