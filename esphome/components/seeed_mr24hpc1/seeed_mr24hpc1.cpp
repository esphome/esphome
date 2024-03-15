#include "seeed_mr24hpc1.h"

#include "esphome/core/log.h"

#include <utility>

namespace esphome {
namespace seeed_mr24hpc1 {

static const char *const TAG = "seeed_mr24hpc1";

// Prints the component's configuration data. dump_config() prints all of the component's configuration
// items in an easy-to-read format, including the configuration key-value pairs.
void MR24HPC1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR24HPC1:");
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR(" ", "Heartbeat Text Sensor", this->heartbeat_state_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Product Model Text Sensor", this->product_model_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Product ID Text Sensor", this->product_id_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Hardware Model Text Sensor", this->hardware_model_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Firware Verison Text Sensor", this->firware_version_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Keep Away Text Sensor", this->keep_away_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Motion Status Text Sensor", this->motion_status_text_sensor_);
  LOG_TEXT_SENSOR(" ", "Custom Mode End Text Sensor", this->custom_mode_end_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR(" ", "Has Target Binary Sensor", this->has_target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR(" ", "Custom Presence Of Detection Sensor", this->custom_presence_of_detection_sensor_);
  LOG_SENSOR(" ", "Movement Signs Sensor", this->movement_signs_sensor_);
  LOG_SENSOR(" ", "Custom Motion Distance Sensor", this->custom_motion_distance_sensor_);
  LOG_SENSOR(" ", "Custom Spatial Static Sensor", this->custom_spatial_static_value_sensor_);
  LOG_SENSOR(" ", "Custom Spatial Motion Sensor", this->custom_spatial_motion_value_sensor_);
  LOG_SENSOR(" ", "Custom Motion Speed Sensor", this->custom_motion_speed_sensor_);
  LOG_SENSOR(" ", "Custom Mode Num Sensor", this->custom_mode_num_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH(" ", "Underly Open Function Switch", this->underlying_open_function_switch_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON(" ", "Restart Button", this->restart_button_);
  LOG_BUTTON(" ", "Custom Set End Button", this->custom_set_end_button_);
#endif
#ifdef USE_SELECT
  LOG_SELECT(" ", "Scene Mode Select", this->scene_mode_select_);
  LOG_SELECT(" ", "Unman Time Select", this->unman_time_select_);
  LOG_SELECT(" ", "Existence Boundary Select", this->existence_boundary_select_);
  LOG_SELECT(" ", "Motion Boundary Select", this->motion_boundary_select_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER(" ", "Sensitivity Number", this->sensitivity_number_);
  LOG_NUMBER(" ", "Custom Mode Number", this->custom_mode_number_);
  LOG_NUMBER(" ", "Existence Threshold Number", this->existence_threshold_number_);
  LOG_NUMBER(" ", "Motion Threshold Number", this->motion_threshold_number_);
  LOG_NUMBER(" ", "Motion Trigger Time Number", this->motion_trigger_number_);
  LOG_NUMBER(" ", "Motion To Rest Time Number", this->motion_to_rest_number_);
  LOG_NUMBER(" ", "Custom Unman Time Number", this->custom_unman_time_number_);
#endif
}

// Initialisation functions
void MR24HPC1Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MR24HPC1...");
  this->check_uart_settings(115200);

  if (this->custom_mode_number_ != nullptr) {
    this->custom_mode_number_->publish_state(0);  // Zero out the custom mode
  }
  if (this->custom_mode_num_sensor_ != nullptr) {
    this->custom_mode_num_sensor_->publish_state(0);
  }
  if (this->custom_mode_end_text_sensor_ != nullptr) {
    this->custom_mode_end_text_sensor_->publish_state("Not in custom mode");
  }
  this->set_custom_end_mode();
  this->poll_time_base_func_check_ = true;
  this->check_dev_inf_sign_ = true;
  this->sg_start_query_data_ = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
  this->sg_data_len_ = 0;
  this->sg_frame_len_ = 0;
  this->sg_recv_data_state_ = FRAME_IDLE;
  this->s_output_info_switch_flag_ = OUTPUT_SWITCH_INIT;

  memset(this->c_product_mode_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_product_id_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_firmware_version_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_hardware_model_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->sg_frame_prase_buf_, 0, FRAME_BUF_MAX_SIZE);
  memset(this->sg_frame_buf_, 0, FRAME_BUF_MAX_SIZE);

  this->set_interval(8000, [this]() { this->update_(); });
  ESP_LOGCONFIG(TAG, "Set up MR24HPC1 complete");
}

// Timed polling of radar data
void MR24HPC1Component::update_() {
  this->get_radar_output_information_switch();  // Query the key status every so often
  this->poll_time_base_func_check_ = true;      // Query the base functionality information at regular intervals
}

// main loop
void MR24HPC1Component::loop() {
  uint8_t byte;

  // Is there data on the serial port
  while (this->available()) {
    this->read_byte(&byte);
    this->r24_split_data_frame_(byte);  // split data frame
  }

  if ((this->s_output_info_switch_flag_ == OUTPUT_SWTICH_OFF) &&
      (this->sg_start_query_data_ > CUSTOM_FUNCTION_QUERY_TIME_OF_ENTER_UNMANNED) && (!this->check_dev_inf_sign_)) {
    this->sg_start_query_data_ = STANDARD_FUNCTION_QUERY_SCENE_MODE;
  } else if ((this->s_output_info_switch_flag_ == OUTPUT_SWITCH_ON) &&
             (this->sg_start_query_data_ < CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY) && (!this->check_dev_inf_sign_)) {
    this->sg_start_query_data_ = CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY;
  } else if (this->check_dev_inf_sign_ && (this->sg_start_query_data_ > STANDARD_FUNCTION_QUERY_HARDWARE_MODE)) {
    // First time power up information polling
    this->sg_start_query_data_ = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
  }

  // Polling Functions
  if (this->poll_time_base_func_check_) {
    switch (this->sg_start_query_data_) {
      case STANDARD_FUNCTION_QUERY_PRODUCT_MODE:
        this->get_product_mode();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_PRODUCT_ID:
        this->get_product_id();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_FIRMWARE_VERSION:
        this->get_product_mode();
        this->get_product_id();
        this->get_firmware_version();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_HARDWARE_MODE:  // Above is the equipment information
        this->get_product_mode();
        this->get_product_id();
        this->get_hardware_model();
        this->sg_start_query_data_++;
        this->check_dev_inf_sign_ = false;
        break;
      case STANDARD_FUNCTION_QUERY_SCENE_MODE:
        this->get_scene_mode();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_SENSITIVITY:
        this->get_sensitivity();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_UNMANNED_TIME:
        this->get_unmanned_time();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_HUMAN_STATUS:
        this->get_human_status();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_HUMAN_MOTION_INF:
        this->get_human_motion_info();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_BODY_MOVE_PARAMETER:
        this->get_body_motion_params();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_KEEPAWAY_STATUS:  // The above is the basic functional information
        this->get_keep_away();
        this->sg_start_query_data_++;
        break;
      case STANDARD_QUERY_CUSTOM_MODE:
        this->get_custom_mode();
        this->sg_start_query_data_++;
        break;
      case STANDARD_FUNCTION_QUERY_HEARTBEAT_STATE:
        this->get_heartbeat_packet();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY:
        this->get_existence_boundary();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_BOUNDARY:
        this->get_motion_boundary();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_EXISTENCE_THRESHOLD:
        this->get_existence_threshold();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_THRESHOLD:
        this->get_motion_threshold();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_TRIGGER_TIME:
        this->get_motion_trigger_time();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_TO_REST_TIME:
        this->get_motion_to_rest_time();
        this->sg_start_query_data_++;
        break;
      case CUSTOM_FUNCTION_QUERY_TIME_OF_ENTER_UNMANNED:
        this->get_custom_unman_time();
        this->sg_start_query_data_++;
        if (this->s_output_info_switch_flag_ == OUTPUT_SWTICH_OFF) {
          this->poll_time_base_func_check_ = false;  // Avoiding high-speed polling that can cause the device to jam
        }
        break;
      case UNDERLY_FUNCTION_QUERY_HUMAN_STATUS:
        this->get_human_status();
        this->sg_start_query_data_++;
        break;
      case UNDERLY_FUNCTION_QUERY_SPATIAL_STATIC_VALUE:
        this->get_spatial_static_value();
        this->sg_start_query_data_++;
        break;
      case UNDERLY_FUNCTION_QUERY_SPATIAL_MOTION_VALUE:
        this->get_spatial_motion_value();
        this->sg_start_query_data_++;
        break;
      case UNDERLY_FUNCTION_QUERY_DISTANCE_OF_STATIC_OBJECT:
        this->get_distance_of_static_object();
        this->sg_start_query_data_++;
        break;
      case UNDERLY_FUNCTION_QUERY_DISTANCE_OF_MOVING_OBJECT:
        this->get_distance_of_moving_object();
        this->sg_start_query_data_++;
        break;
      case UNDERLY_FUNCTION_QUERY_TARGET_MOVEMENT_SPEED:
        this->get_target_movement_speed();
        this->sg_start_query_data_++;
        this->poll_time_base_func_check_ = false;  // Avoiding high-speed polling that can cause the device to jam
        break;
      default:
        break;
    }
  }
}

// Calculate CRC check digit
static uint8_t get_frame_crc_sum(const uint8_t *data, int len) {
  unsigned int crc_sum = 0;
  for (int i = 0; i < len - 3; i++) {
    crc_sum += data[i];
  }
  return crc_sum & 0xff;
}

// Check that the check digit is correct
static int get_frame_check_status(uint8_t *data, int len) {
  uint8_t crc_sum = get_frame_crc_sum(data, len);
  uint8_t verified = data[len - 3];
  return (verified == crc_sum) ? 1 : 0;
}

// split data frame
void MR24HPC1Component::r24_split_data_frame_(uint8_t value) {
  switch (this->sg_recv_data_state_) {
    case FRAME_IDLE:  // starting value
      if (FRAME_HEADER1_VALUE == value) {
        this->sg_recv_data_state_ = FRAME_HEADER2;
      }
      break;
    case FRAME_HEADER2:
      if (FRAME_HEADER2_VALUE == value) {
        this->sg_frame_buf_[0] = FRAME_HEADER1_VALUE;
        this->sg_frame_buf_[1] = FRAME_HEADER2_VALUE;
        this->sg_recv_data_state_ = FRAME_CTL_WORD;
      } else {
        this->sg_recv_data_state_ = FRAME_IDLE;
        ESP_LOGD(TAG, "FRAME_IDLE ERROR value:%x", value);
      }
      break;
    case FRAME_CTL_WORD:
      this->sg_frame_buf_[2] = value;
      this->sg_recv_data_state_ = FRAME_CMD_WORD;
      break;
    case FRAME_CMD_WORD:
      this->sg_frame_buf_[3] = value;
      this->sg_recv_data_state_ = FRAME_DATA_LEN_H;
      break;
    case FRAME_DATA_LEN_H:
      if (value <= 4) {
        this->sg_data_len_ = value * 256;
        this->sg_frame_buf_[4] = value;
        this->sg_recv_data_state_ = FRAME_DATA_LEN_L;
      } else {
        this->sg_data_len_ = 0;
        this->sg_recv_data_state_ = FRAME_IDLE;
        ESP_LOGD(TAG, "FRAME_DATA_LEN_H ERROR value:%x", value);
      }
      break;
    case FRAME_DATA_LEN_L:
      this->sg_data_len_ += value;
      if (this->sg_data_len_ > 32) {
        ESP_LOGD(TAG, "len=%d, FRAME_DATA_LEN_L ERROR value:%x", this->sg_data_len_, value);
        this->sg_data_len_ = 0;
        this->sg_recv_data_state_ = FRAME_IDLE;
      } else {
        this->sg_frame_buf_[5] = value;
        this->sg_frame_len_ = 6;
        this->sg_recv_data_state_ = FRAME_DATA_BYTES;
      }
      break;
    case FRAME_DATA_BYTES:
      this->sg_data_len_ -= 1;
      this->sg_frame_buf_[this->sg_frame_len_++] = value;
      if (this->sg_data_len_ <= 0) {
        this->sg_recv_data_state_ = FRAME_DATA_CRC;
      }
      break;
    case FRAME_DATA_CRC:
      this->sg_frame_buf_[this->sg_frame_len_++] = value;
      this->sg_recv_data_state_ = FRAME_TAIL1;
      break;
    case FRAME_TAIL1:
      if (FRAME_TAIL1_VALUE == value) {
        this->sg_recv_data_state_ = FRAME_TAIL2;
      } else {
        this->sg_recv_data_state_ = FRAME_IDLE;
        this->sg_frame_len_ = 0;
        this->sg_data_len_ = 0;
        ESP_LOGD(TAG, "FRAME_TAIL1 ERROR value:%x", value);
      }
      break;
    case FRAME_TAIL2:
      if (FRAME_TAIL2_VALUE == value) {
        this->sg_frame_buf_[this->sg_frame_len_++] = FRAME_TAIL1_VALUE;
        this->sg_frame_buf_[this->sg_frame_len_++] = FRAME_TAIL2_VALUE;
        memcpy(this->sg_frame_prase_buf_, this->sg_frame_buf_, this->sg_frame_len_);
        if (get_frame_check_status(this->sg_frame_prase_buf_, this->sg_frame_len_)) {
          this->r24_parse_data_frame_(this->sg_frame_prase_buf_, this->sg_frame_len_);
        } else {
          ESP_LOGD(TAG, "frame check failer!");
        }
      } else {
        ESP_LOGD(TAG, "FRAME_TAIL2 ERROR value:%x", value);
      }
      memset(this->sg_frame_prase_buf_, 0, FRAME_BUF_MAX_SIZE);
      memset(this->sg_frame_buf_, 0, FRAME_BUF_MAX_SIZE);
      this->sg_frame_len_ = 0;
      this->sg_data_len_ = 0;
      this->sg_recv_data_state_ = FRAME_IDLE;
      break;
    default:
      this->sg_recv_data_state_ = FRAME_IDLE;
  }
}

// Parses data frames related to product information
void MR24HPC1Component::r24_frame_parse_product_information_(uint8_t *data) {
  uint16_t product_len = encode_uint16(data[FRAME_COMMAND_WORD_INDEX + 1], data[FRAME_COMMAND_WORD_INDEX + 2]);
  if (data[FRAME_COMMAND_WORD_INDEX] == COMMAND_PRODUCT_MODE) {
    if ((this->product_model_text_sensor_ != nullptr) && (product_len < PRODUCT_BUF_MAX_SIZE)) {
      memset(this->c_product_mode_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_product_mode_, &data[FRAME_DATA_INDEX], product_len);
      this->product_model_text_sensor_->publish_state(this->c_product_mode_);
    } else {
      ESP_LOGD(TAG, "Reply: get product_mode error!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == COMMAND_PRODUCT_ID) {
    if ((this->product_id_text_sensor_ != nullptr) && (product_len < PRODUCT_BUF_MAX_SIZE)) {
      memset(this->c_product_id_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_product_id_, &data[FRAME_DATA_INDEX], product_len);
      this->product_id_text_sensor_->publish_state(this->c_product_id_);
    } else {
      ESP_LOGD(TAG, "Reply: get productId error!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == COMMAND_HARDWARE_MODEL) {
    if ((this->hardware_model_text_sensor_ != nullptr) && (product_len < PRODUCT_BUF_MAX_SIZE)) {
      memset(this->c_hardware_model_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_hardware_model_, &data[FRAME_DATA_INDEX], product_len);
      this->hardware_model_text_sensor_->publish_state(this->c_hardware_model_);
      ESP_LOGD(TAG, "Reply: get hardware_model :%s", this->c_hardware_model_);
    } else {
      ESP_LOGD(TAG, "Reply: get hardwareModel error!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == COMMAND_FIRMWARE_VERSION) {
    if ((this->firware_version_text_sensor_ != nullptr) && (product_len < PRODUCT_BUF_MAX_SIZE)) {
      memset(this->c_firmware_version_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_firmware_version_, &data[FRAME_DATA_INDEX], product_len);
      this->firware_version_text_sensor_->publish_state(this->c_firmware_version_);
    } else {
      ESP_LOGD(TAG, "Reply: get firmwareVersion error!");
    }
  }
}

// Parsing the underlying open parameters
void MR24HPC1Component::r24_frame_parse_open_underlying_information_(uint8_t *data) {
  if (data[FRAME_COMMAND_WORD_INDEX] == 0x00) {
    if (this->underlying_open_function_switch_ != nullptr) {
      this->underlying_open_function_switch_->publish_state(
          data[FRAME_DATA_INDEX]);  // Underlying Open Parameter Switch Status Updates
    }
    if (data[FRAME_DATA_INDEX]) {
      this->s_output_info_switch_flag_ = OUTPUT_SWITCH_ON;
    } else {
      this->s_output_info_switch_flag_ = OUTPUT_SWTICH_OFF;
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
    if (this->custom_spatial_static_value_sensor_ != nullptr) {
      this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    if (this->custom_presence_of_detection_sensor_ != nullptr) {
      this->custom_presence_of_detection_sensor_->publish_state(data[FRAME_DATA_INDEX + 1] * 0.5f);
    }
    if (this->custom_spatial_motion_value_sensor_ != nullptr) {
      this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX + 2]);
    }
    if (this->custom_motion_distance_sensor_ != nullptr) {
      this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX + 3] * 0.5f);
    }
    if (this->custom_motion_speed_sensor_ != nullptr) {
      this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX + 4] - 10) * 0.5f);
    }
  } else if ((data[FRAME_COMMAND_WORD_INDEX] == 0x06) || (data[FRAME_COMMAND_WORD_INDEX] == 0x86)) {
    // none:0x00  close_to:0x01  far_away:0x02
    if ((this->keep_away_text_sensor_ != nullptr) && (data[FRAME_DATA_INDEX] < 3)) {
      this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if ((this->movement_signs_sensor_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x07) || (data[FRAME_COMMAND_WORD_INDEX] == 0x87))) {
    this->movement_signs_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->existence_threshold_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x08) || (data[FRAME_COMMAND_WORD_INDEX] == 0x88))) {
    this->existence_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->motion_threshold_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x09) || (data[FRAME_COMMAND_WORD_INDEX] == 0x89))) {
    this->motion_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->existence_boundary_select_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0a) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8a))) {
    if (this->existence_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->existence_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if ((this->motion_boundary_select_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0b) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8b))) {
    if (this->motion_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->motion_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if ((this->motion_trigger_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0c) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8c))) {
    uint32_t motion_trigger_time = encode_uint32(data[FRAME_DATA_INDEX], data[FRAME_DATA_INDEX + 1],
                                                 data[FRAME_DATA_INDEX + 2], data[FRAME_DATA_INDEX + 3]);
    this->motion_trigger_number_->publish_state(motion_trigger_time);
  } else if ((this->motion_to_rest_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0d) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8d))) {
    uint32_t move_to_rest_time = encode_uint32(data[FRAME_DATA_INDEX], data[FRAME_DATA_INDEX + 1],
                                               data[FRAME_DATA_INDEX + 2], data[FRAME_DATA_INDEX + 3]);
    this->motion_to_rest_number_->publish_state(move_to_rest_time);
  } else if ((this->custom_unman_time_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0e) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8e))) {
    uint32_t enter_unmanned_time = encode_uint32(data[FRAME_DATA_INDEX], data[FRAME_DATA_INDEX + 1],
                                                 data[FRAME_DATA_INDEX + 2], data[FRAME_DATA_INDEX + 3]);
    float custom_unmanned_time = enter_unmanned_time / 1000.0;
    this->custom_unman_time_number_->publish_state(custom_unmanned_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x80) {
    if (data[FRAME_DATA_INDEX]) {
      this->s_output_info_switch_flag_ = OUTPUT_SWITCH_ON;
    } else {
      this->s_output_info_switch_flag_ = OUTPUT_SWTICH_OFF;
    }
    if (this->underlying_open_function_switch_ != nullptr) {
      this->underlying_open_function_switch_->publish_state(data[FRAME_DATA_INDEX]);
    }
  } else if ((this->custom_spatial_static_value_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x81)) {
    this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->custom_spatial_motion_value_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x82)) {
    this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->custom_presence_of_detection_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x83)) {
    this->custom_presence_of_detection_sensor_->publish_state(
        S_PRESENCE_OF_DETECTION_RANGE_STR[data[FRAME_DATA_INDEX]]);
  } else if ((this->custom_motion_distance_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x84)) {
    this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX] * 0.5f);
  } else if ((this->custom_motion_speed_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x85)) {
    this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX] - 10) * 0.5f);
  }
}

void MR24HPC1Component::r24_parse_data_frame_(uint8_t *data, uint8_t len) {
  switch (data[FRAME_CONTROL_WORD_INDEX]) {
    case 0x01: {
      if ((this->heartbeat_state_text_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x01)) {
        this->heartbeat_state_text_sensor_->publish_state("Equipment Normal");
      } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02) {
        ESP_LOGD(TAG, "Reply: query restart packet");
      } else if (this->heartbeat_state_text_sensor_ != nullptr) {
        this->heartbeat_state_text_sensor_->publish_state("Equipment Abnormal");
      }
    } break;
    case 0x02: {
      this->r24_frame_parse_product_information_(data);
    } break;
    case 0x05: {
      this->r24_frame_parse_work_status_(data);
    } break;
    case 0x08: {
      this->r24_frame_parse_open_underlying_information_(data);
    } break;
    case 0x80: {
      this->r24_frame_parse_human_information_(data);
    } break;
    default:
      ESP_LOGD(TAG, "control word:0x%02X not found", data[FRAME_CONTROL_WORD_INDEX]);
      break;
  }
}

void MR24HPC1Component::r24_frame_parse_work_status_(uint8_t *data) {
  if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
    ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07) {
    if ((this->scene_mode_select_ != nullptr) && (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX]))) {
      this->scene_mode_select_->publish_state(S_SCENE_STR[data[FRAME_DATA_INDEX]]);
    } else {
      ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
    }
  } else if ((this->sensitivity_number_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x08) || (data[FRAME_COMMAND_WORD_INDEX] == 0x88))) {
    // 1-3
    this->sensitivity_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09) {
    // 1-4
    if (this->custom_mode_num_sensor_ != nullptr) {
      this->custom_mode_num_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    if (this->custom_mode_number_ != nullptr) {
      this->custom_mode_number_->publish_state(0);
    }
    if (this->custom_mode_end_text_sensor_ != nullptr) {
      this->custom_mode_end_text_sensor_->publish_state("Setup in progress...");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
    ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87) {
    if ((this->scene_mode_select_ != nullptr) && (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX]))) {
      this->scene_mode_select_->publish_state(S_SCENE_STR[data[FRAME_DATA_INDEX]]);
    } else {
      ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
    }
  } else if ((this->custom_mode_end_text_sensor_ != nullptr) && (data[FRAME_COMMAND_WORD_INDEX] == 0x0A)) {
    this->custom_mode_end_text_sensor_->publish_state("Set Success!");
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89) {
    if (data[FRAME_DATA_INDEX] == 0) {
      if (this->custom_mode_end_text_sensor_ != nullptr) {
        this->custom_mode_end_text_sensor_->publish_state("Not in custom mode");
      }
      if (this->custom_mode_number_ != nullptr) {
        this->custom_mode_number_->publish_state(0);
      }
      if (this->custom_mode_num_sensor_ != nullptr) {
        this->custom_mode_num_sensor_->publish_state(data[FRAME_DATA_INDEX]);
      }
    } else {
      if (this->custom_mode_num_sensor_ != nullptr) {
        this->custom_mode_num_sensor_->publish_state(data[FRAME_DATA_INDEX]);
      }
    }
  } else {
    ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
  }
}

void MR24HPC1Component::r24_frame_parse_human_information_(uint8_t *data) {
  if ((this->has_target_binary_sensor_ != nullptr) &&
      ((data[FRAME_COMMAND_WORD_INDEX] == 0x01) || (data[FRAME_COMMAND_WORD_INDEX] == 0x81))) {
    this->has_target_binary_sensor_->publish_state(S_SOMEONE_EXISTS_STR[data[FRAME_DATA_INDEX]]);
  } else if ((this->motion_status_text_sensor_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x02) || (data[FRAME_COMMAND_WORD_INDEX] == 0x82))) {
    if (data[FRAME_DATA_INDEX] < 3) {
      this->motion_status_text_sensor_->publish_state(S_MOTION_STATUS_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if ((this->movement_signs_sensor_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x03) || (data[FRAME_COMMAND_WORD_INDEX] == 0x83))) {
    this->movement_signs_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if ((this->unman_time_select_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0A) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8A))) {
    // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
    if (data[FRAME_DATA_INDEX] < 9) {
      this->unman_time_select_->publish_state(S_UNMANNED_TIME_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if ((this->keep_away_text_sensor_ != nullptr) &&
             ((data[FRAME_COMMAND_WORD_INDEX] == 0x0B) || (data[FRAME_COMMAND_WORD_INDEX] == 0x8B))) {
    // none:0x00  close_to:0x01  far_away:0x02
    if (data[FRAME_DATA_INDEX] < 3) {
      this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
    }
  } else {
    ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
  }
}

// Sending data frames
void MR24HPC1Component::send_query_(const uint8_t *query, size_t string_length) {
  this->write_array(query, string_length);
}

// Send Heartbeat Packet Command
void MR24HPC1Component::get_heartbeat_packet() { this->send_query_(GET_HEARTBEAT, sizeof(GET_HEARTBEAT)); }

// Issuance of the underlying open parameter query command
void MR24HPC1Component::get_radar_output_information_switch() {
  this->send_query_(GET_RADAR_OUTPUT_INFORMATION_SWITCH, sizeof(GET_RADAR_OUTPUT_INFORMATION_SWITCH));
}

// Issuance of product model orders
void MR24HPC1Component::get_product_mode() { this->send_query_(GET_PRODUCT_MODE, sizeof(GET_PRODUCT_MODE)); }

// Issuing the Get Product ID command
void MR24HPC1Component::get_product_id() { this->send_query_(GET_PRODUCT_ID, sizeof(GET_PRODUCT_ID)); }

// Issuing hardware model commands
void MR24HPC1Component::get_hardware_model() { this->send_query_(GET_HARDWARE_MODEL, sizeof(GET_HARDWARE_MODEL)); }

// Issuing software version commands
void MR24HPC1Component::get_firmware_version() {
  this->send_query_(GET_FIRMWARE_VERSION, sizeof(GET_FIRMWARE_VERSION));
}

void MR24HPC1Component::get_human_status() { this->send_query_(GET_HUMAN_STATUS, sizeof(GET_HUMAN_STATUS)); }

void MR24HPC1Component::get_human_motion_info() {
  this->send_query_(GET_HUMAN_MOTION_INFORMATION, sizeof(GET_HUMAN_MOTION_INFORMATION));
}

void MR24HPC1Component::get_body_motion_params() {
  this->send_query_(GET_BODY_MOTION_PARAMETERS, sizeof(GET_BODY_MOTION_PARAMETERS));
}

void MR24HPC1Component::get_keep_away() { this->send_query_(GET_KEEP_AWAY, sizeof(GET_KEEP_AWAY)); }

void MR24HPC1Component::get_scene_mode() { this->send_query_(GET_SCENE_MODE, sizeof(GET_SCENE_MODE)); }

void MR24HPC1Component::get_sensitivity() { this->send_query_(GET_SENSITIVITY, sizeof(GET_SENSITIVITY)); }

void MR24HPC1Component::get_unmanned_time() { this->send_query_(GET_UNMANNED_TIME, sizeof(GET_UNMANNED_TIME)); }

void MR24HPC1Component::get_custom_mode() { this->send_query_(GET_CUSTOM_MODE, sizeof(GET_CUSTOM_MODE)); }

void MR24HPC1Component::get_existence_boundary() {
  this->send_query_(GET_EXISTENCE_BOUNDARY, sizeof(GET_EXISTENCE_BOUNDARY));
}

void MR24HPC1Component::get_motion_boundary() { this->send_query_(GET_MOTION_BOUNDARY, sizeof(GET_MOTION_BOUNDARY)); }

void MR24HPC1Component::get_spatial_static_value() {
  this->send_query_(GET_SPATIAL_STATIC_VALUE, sizeof(GET_SPATIAL_STATIC_VALUE));
}

void MR24HPC1Component::get_spatial_motion_value() {
  this->send_query_(GET_SPATIAL_MOTION_VALUE, sizeof(GET_SPATIAL_MOTION_VALUE));
}

void MR24HPC1Component::get_distance_of_static_object() {
  this->send_query_(GET_DISTANCE_OF_STATIC_OBJECT, sizeof(GET_DISTANCE_OF_STATIC_OBJECT));
}

void MR24HPC1Component::get_distance_of_moving_object() {
  this->send_query_(GET_DISTANCE_OF_MOVING_OBJECT, sizeof(GET_DISTANCE_OF_MOVING_OBJECT));
}

void MR24HPC1Component::get_target_movement_speed() {
  this->send_query_(GET_TARGET_MOVEMENT_SPEED, sizeof(GET_TARGET_MOVEMENT_SPEED));
}

void MR24HPC1Component::get_existence_threshold() {
  this->send_query_(GET_EXISTENCE_THRESHOLD, sizeof(GET_EXISTENCE_THRESHOLD));
}

void MR24HPC1Component::get_motion_threshold() {
  this->send_query_(GET_MOTION_THRESHOLD, sizeof(GET_MOTION_THRESHOLD));
}

void MR24HPC1Component::get_motion_trigger_time() {
  this->send_query_(GET_MOTION_TRIGGER_TIME, sizeof(GET_MOTION_TRIGGER_TIME));
}

void MR24HPC1Component::get_motion_to_rest_time() {
  this->send_query_(GET_MOTION_TO_REST_TIME, sizeof(GET_MOTION_TO_REST_TIME));
}

void MR24HPC1Component::get_custom_unman_time() {
  this->send_query_(GET_CUSTOM_UNMAN_TIME, sizeof(GET_CUSTOM_UNMAN_TIME));
}

// Logic of setting: After setting, query whether the setting is successful or not!

void MR24HPC1Component::set_underlying_open_function(bool enable) {
  if (enable) {
    this->send_query_(UNDERLYING_SWITCH_ON, sizeof(UNDERLYING_SWITCH_ON));
  } else {
    this->send_query_(UNDERLYING_SWITCH_OFF, sizeof(UNDERLYING_SWITCH_OFF));
  }
  if (this->keep_away_text_sensor_ != nullptr) {
    this->keep_away_text_sensor_->publish_state("");
  }
  if (this->motion_status_text_sensor_ != nullptr) {
    this->motion_status_text_sensor_->publish_state("");
  }
  if (this->custom_spatial_static_value_sensor_ != nullptr) {
    this->custom_spatial_static_value_sensor_->publish_state(NAN);
  }
  if (this->custom_spatial_motion_value_sensor_ != nullptr) {
    this->custom_spatial_motion_value_sensor_->publish_state(NAN);
  }
  if (this->custom_motion_distance_sensor_ != nullptr) {
    this->custom_motion_distance_sensor_->publish_state(NAN);
  }
  if (this->custom_presence_of_detection_sensor_ != nullptr) {
    this->custom_presence_of_detection_sensor_->publish_state(NAN);
  }
  if (this->custom_motion_speed_sensor_ != nullptr) {
    this->custom_motion_speed_sensor_->publish_state(NAN);
  }
}

void MR24HPC1Component::set_scene_mode(uint8_t value) {
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x07, 0x00, 0x01, value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  if (this->custom_mode_number_ != nullptr) {
    this->custom_mode_number_->publish_state(0);
  }
  if (this->custom_mode_num_sensor_ != nullptr) {
    this->custom_mode_num_sensor_->publish_state(0);
  }
  this->get_scene_mode();
  this->get_sensitivity();
  this->get_custom_mode();
  this->get_existence_boundary();
  this->get_motion_boundary();
  this->get_existence_threshold();
  this->get_motion_threshold();
  this->get_motion_trigger_time();
  this->get_motion_to_rest_time();
  this->get_custom_unman_time();
}

void MR24HPC1Component::set_sensitivity(uint8_t value) {
  if (value == 0x00)
    return;
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x08, 0x00, 0x01, value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_scene_mode();
  this->get_sensitivity();
}

void MR24HPC1Component::set_restart() {
  this->send_query_(SET_RESTART, sizeof(SET_RESTART));
  this->check_dev_inf_sign_ = true;
}

void MR24HPC1Component::set_unman_time(uint8_t value) {
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x80, 0x0a, 0x00, 0x01, value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_unmanned_time();
}

void MR24HPC1Component::set_custom_mode(uint8_t mode) {
  if (mode == 0) {
    this->set_custom_end_mode();  // Equivalent to end setting
    if (this->custom_mode_number_ != nullptr) {
      this->custom_mode_number_->publish_state(0);
    }
    return;
  }
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x09, 0x00, 0x01, mode, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_existence_boundary();
  this->get_motion_boundary();
  this->get_existence_threshold();
  this->get_motion_threshold();
  this->get_motion_trigger_time();
  this->get_motion_to_rest_time();
  this->get_custom_unman_time();
  this->get_custom_mode();
  this->get_scene_mode();
  this->get_sensitivity();
}

void MR24HPC1Component::set_custom_end_mode() {
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x0a, 0x00, 0x01, 0x0F, 0xCB, 0x54, 0x43};
  this->send_query_(send_data, send_data_len);
  if (this->custom_mode_number_ != nullptr) {
    this->custom_mode_number_->publish_state(0);  // Clear setpoints
  }
  this->get_existence_boundary();
  this->get_motion_boundary();
  this->get_existence_threshold();
  this->get_motion_threshold();
  this->get_motion_trigger_time();
  this->get_motion_to_rest_time();
  this->get_custom_unman_time();
  this->get_custom_mode();
  this->get_scene_mode();
  this->get_sensitivity();
}

void MR24HPC1Component::set_existence_boundary(uint8_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x0A, 0x00, 0x01, (uint8_t) (value + 1), 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_existence_boundary();
}

void MR24HPC1Component::set_motion_boundary(uint8_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x0B, 0x00, 0x01, (uint8_t) (value + 1), 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_motion_boundary();
}

void MR24HPC1Component::set_existence_threshold(uint8_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x08, 0x00, 0x01, value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_existence_threshold();
}

void MR24HPC1Component::set_motion_threshold(uint8_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x09, 0x00, 0x01, value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_motion_threshold();
}

void MR24HPC1Component::set_motion_trigger_time(uint8_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t send_data_len = 13;
  uint8_t send_data[13] = {0x53, 0x59, 0x08, 0x0C, 0x00, 0x04, 0x00, 0x00, 0x00, value, 0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_motion_trigger_time();
}

void MR24HPC1Component::set_motion_to_rest_time(uint16_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t h8_num = (value >> 8) & 0xff;
  uint8_t l8_num = value & 0xff;
  uint8_t send_data_len = 13;
  uint8_t send_data[13] = {0x53, 0x59, 0x08, 0x0D, 0x00, 0x04, 0x00, 0x00, h8_num, l8_num, 0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_motion_to_rest_time();
}

void MR24HPC1Component::set_custom_unman_time(uint16_t value) {
  if ((this->custom_mode_num_sensor_ != nullptr) && (this->custom_mode_num_sensor_->state == 0))
    return;  // You'll have to check that you're in custom mode to set it up
  uint32_t value_ms = value * 1000;
  uint8_t h24_num = (value_ms >> 24) & 0xff;
  uint8_t h16_num = (value_ms >> 16) & 0xff;
  uint8_t h8_num = (value_ms >> 8) & 0xff;
  uint8_t l8_num = value_ms & 0xff;
  uint8_t send_data_len = 13;
  uint8_t send_data[13] = {0x53, 0x59, 0x08, 0x0E, 0x00, 0x04, h24_num, h16_num, h8_num, l8_num, 0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query_(send_data, send_data_len);
  this->get_custom_unman_time();
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
