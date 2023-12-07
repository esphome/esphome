#include "esphome/core/log.h"
#include "seeed_mr24hpc1.h"

#include <utility>
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace seeed_mr24hpc1 {

static const char *TAG = "seeed_mr24hpc1";
int sg_start_query_data;
bool check_dev_inf_sign;
bool poll_time_base_func_check;

static uint8_t s_output_info_switch_flag = OUTPUT_SWITCH_INIT;
static uint8_t sg_recv_data_state = FRAME_IDLE;
static uint8_t sg_frame_len = 0;
static uint8_t sg_data_len = 0;
static uint8_t sg_frame_buf[FRAME_BUF_MAX_SIZE] = {0};
static uint8_t sg_frame_prase_buf[FRAME_BUF_MAX_SIZE] = {0};

// Prints the component's configuration data. dump_config() prints all of the component's configuration
// items in an easy-to-read format, including the configuration key-value pairs.
void MR24HPC1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR24HPC1:");
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR(" ", "HeartbeatTextSensor", this->heartbeat_state_text_sensor_);
  LOG_TEXT_SENSOR(" ", "ProductModelTextSensor", this->product_model_text_sensor_);
  LOG_TEXT_SENSOR(" ", "ProductIDTextSensor", this->product_id_text_sensor_);
  LOG_TEXT_SENSOR(" ", "HardwareModelTextSensor", this->hardware_model_text_sensor_);
  LOG_TEXT_SENSOR(" ", "FirwareVerisonTextSensor", this->firware_version_text_sensor_);
  LOG_TEXT_SENSOR(" ", "KeepAwaySensor", this->keep_away_text_sensor_);
  LOG_TEXT_SENSOR(" ", "MotionStatusSensor", this->motion_status_text_sensor_);
  LOG_TEXT_SENSOR(" ", "CustomModeEnd", this->custom_mode_end_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR(" ", "SomeoneExistsBinarySensor", this->someoneExists_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR(" ", "CustomPresenceOfDetectionSensor", this->custom_presence_of_detection_sensor_);
  LOG_SENSOR(" ", "movementsigns", this->movementSigns_sensor_);
  LOG_SENSOR(" ", "custommotiondistance", this->custom_motion_distance_sensor_);
  LOG_SENSOR(" ", "customspatialstaticvalue", this->custom_spatial_static_value_sensor_);
  LOG_SENSOR(" ", "customspatialmotionvalue", this->custom_spatial_motion_value_sensor_);
  LOG_SENSOR(" ", "custommotionspeed", this->custom_motion_speed_sensor_);
  LOG_SENSOR(" ", "custommodenum", this->custom_mode_num_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH(" ", "underly_open_function", this->underly_open_function_switch_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON(" ", "ResetButton", this->reset_button_);
  LOG_BUTTON(" ", "CustomSetEnd", this->custom_set_end_button_);
#endif
#ifdef USE_SELECT
  LOG_SELECT(" ", "SceneModeSelect", this->scene_mode_select_);
  LOG_SELECT(" ", "UnmanTimeSelect", this->unman_time_select_);
  LOG_SELECT(" ", "ExistenceBoundarySelect", this->existence_boundary_select_);
  LOG_SELECT(" ", "MotionBoundarySelect", this->motion_boundary_select_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER(" ", "SensitivityNumber", this->sensitivity_number_);
  LOG_NUMBER(" ", "CustomModeNumber", this->custom_mode_number_);
  LOG_NUMBER(" ", "ExistenceThresholdNumber", this->existence_threshold_number_);
  LOG_NUMBER(" ", "MotionThresholdNumber", this->motion_threshold_number_);
  LOG_NUMBER(" ", "MotionTriggerTimeNumber", this->motion_trigger_number_);
  LOG_NUMBER(" ", "Motion2RestTimeNumber", this->motion_to_rest_number_);
  LOG_NUMBER(" ", "CustomUnmanTimeNumber", this->custom_unman_time_number_);
#endif
}

// Initialisation functions
void MR24HPC1Component::setup() {
  ESP_LOGCONFIG(TAG, "uart_settings is 115200");
  this->check_uart_settings(115200);
  this->custom_mode_number_->publish_state(0);  // Zero out the custom mode
  this->custom_mode_num_sensor_->publish_state(0);
  this->custom_mode_end_text_sensor_->publish_state("Not in custom mode");
  this->set_custom_end_mode();
  poll_time_base_func_check = true;
  check_dev_inf_sign = true;
  sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;

  memset(this->c_product_mode_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_product_id_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_firmware_version_, 0, PRODUCT_BUF_MAX_SIZE);
  memset(this->c_hardware_model_, 0, PRODUCT_BUF_MAX_SIZE);
}

// component callback function, which is called every time the loop is called
void MR24HPC1Component::update() {
  this->get_radar_output_information_switch();  // Query the key status every so often
  poll_time_base_func_check = true;             // Query the base functionality information at regular intervals
}

// main loop
void MR24HPC1Component::loop() {
  uint8_t byte;

  // Is there data on the serial port
  while (this->available()) {
    this->read_byte(&byte);
    this->r24_split_data_frame(byte);  // split data frame
  }

  if ((s_output_info_switch_flag == OUTPUT_SWTICH_OFF) &&
      (sg_start_query_data > CUSTOM_FUNCTION_QUERY_TIME_OF_ENTER_UNMANNED) &&
      (!check_dev_inf_sign)) {
    sg_start_query_data = STANDARD_FUNCTION_QUERY_SCENE_MODE;
  } else if ((s_output_info_switch_flag == OUTPUT_SWTICH_ON) &&
             (sg_start_query_data < CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY) &&
             (!check_dev_inf_sign)) {
    sg_start_query_data = CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY;
  } else if (check_dev_inf_sign &&
             (sg_start_query_data > STANDARD_FUNCTION_QUERY_HARDWARE_MODE)) {
              // First time power up information polling
    sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
  }

  // Polling Functions
  if (poll_time_base_func_check) {
    switch (sg_start_query_data) {
      case STANDARD_FUNCTION_QUERY_PRODUCT_MODE:
        this->get_product_mode();
        this->get_product_id();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_PRODUCT_ID:
        this->get_product_mode();
        this->get_product_id();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_FIRMWARE_VERSION:
        this->get_product_mode();
        this->get_product_id();
        this->get_firmware_version();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_HARDWARE_MODE:  // Above is the equipment information
        this->get_product_mode();
        this->get_product_id();
        this->get_hardware_model();
        sg_start_query_data++;
        check_dev_inf_sign = false;
        break;
      case STANDARD_FUNCTION_QUERY_SCENE_MODE:
        this->get_scene_mode();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_SENSITIVITY:
        this->get_sensitivity();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_UNMANNED_TIME:
        this->get_unmanned_time();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_HUMAN_STATUS:
        this->get_human_status();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_HUMAN_MOTION_INF:
        this->get_human_motion_info();
        sg_start_query_data++;
        break;
      // case STANDARD_FUNCTION_QUERY_BODY_MOVE_PARAMETER: // It is not recommended to turn on the query for body
      // movement parameters, as the frequency of reporting is frequent enough
      //   this->get_body_motion_params();
      //   sg_start_query_data++;
      //   break;
      case STANDARD_FUNCTION_QUERY_KEEPAWAY_STATUS:  // The above is the basic functional information
        this->get_keep_away();
        sg_start_query_data++;
        break;
      case STANDARD_QUERY_CUSTOM_MODE:
        this->get_custom_mode();
        sg_start_query_data++;
        break;
      case STANDARD_FUNCTION_QUERY_HEARTBEAT_STATE:
        this->get_heartbeat_packet();
        sg_start_query_data++;
        break;
      // case UNDERLY_FUNCTION_QUERY_SPATIAL_STATIC_VALUE:
      // this->get_spatial_static_value();  // Values reported on a regular basis, so no need turn on
      // sg_start_query_data++;
      // break;
      // case UNDERLY_FUNCTION_QUERY_SPATIAL_MOTION_VALUE:
      // this->get_spatial_motion_value();
      // sg_start_query_data++;
      // break;
      // case UNDERLY_FUNCTION_QUERY_DISTANCE_OF_STATIC_OBJECT:
      // this->get_distance_of_static_object();
      // sg_start_query_data++;
      // break;
      // case UNDERLY_FUNCTION_QUERY_DISTANCE_OF_MOVING_OBJECT:
      // this->get_distance_of_moving_object();
      // sg_start_query_data++;
      // break;
      // case UNDERLY_FUNCTION_QUERY_TARGET_MOVEMENT_SPEED:
      // this->get_target_movement_speed();
      // sg_start_query_data++;
      // break;
      case CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY:
        this->get_existence_boundary();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_BOUNDARY:
        this->get_motion_boundary();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_EXISTENCE_THRESHOLD:
        this->get_existence_threshold();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_THRESHOLD:
        this->get_motion_threshold();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_TRIGGER_TIME:
        this->get_motion_trigger_time();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_MOTION_TO_REST_TIME:
        this->get_motion_to_rest_time();
        sg_start_query_data++;
        break;
      case CUSTOM_FUNCTION_QUERY_TIME_OF_ENTER_UNMANNED:
        this->get_custom_unman_time();
        sg_start_query_data++;
        if (s_output_info_switch_flag == OUTPUT_SWTICH_OFF) {
          poll_time_base_func_check = false;         // Avoiding high-speed polling that can cause the device to jam
        }
        break;
      case UNDERLY_FUNCTION_QUERY_HUMAN_STATUS:
        this->get_human_status();
        sg_start_query_data++;
        if (s_output_info_switch_flag == OUTPUT_SWTICH_ON) {
          poll_time_base_func_check = false;         // Avoiding high-speed polling that can cause the device to jam
        }
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
void MR24HPC1Component::r24_split_data_frame(uint8_t value) {
  switch (sg_recv_data_state) {
    case FRAME_IDLE:  // starting value
      if (FRAME_HEADER1_VALUE == value) {
        sg_recv_data_state = FRAME_HEADER2;
      }
      break;
    case FRAME_HEADER2:
      if (FRAME_HEADER2_VALUE == value) {
        sg_frame_buf[0] = FRAME_HEADER1_VALUE;
        sg_frame_buf[1] = FRAME_HEADER2_VALUE;
        sg_recv_data_state = FRAME_CTL_WORLD;
      } else {
        sg_recv_data_state = FRAME_IDLE;
        ESP_LOGD(TAG, "FRAME_IDLE ERROR value:%x", value);
      }
      break;
    case FRAME_CTL_WORLD:
      sg_frame_buf[2] = value;
      sg_recv_data_state = FRAME_CMD_WORLD;
      break;
    case FRAME_CMD_WORLD:
      sg_frame_buf[3] = value;
      sg_recv_data_state = FRAME_DATA_LEN_H;
      break;
    case FRAME_DATA_LEN_H:
      if (value <= 4) {
        sg_data_len = value * 256;
        sg_frame_buf[4] = value;
        sg_recv_data_state = FRAME_DATA_LEN_L;
      } else {
        sg_data_len = 0;
        sg_recv_data_state = FRAME_IDLE;
        ESP_LOGD(TAG, "FRAME_DATA_LEN_H ERROR value:%x", value);
      }
      break;
    case FRAME_DATA_LEN_L:
      sg_data_len += value;
      if (sg_data_len > 32) {
        ESP_LOGD(TAG, "len=%d, FRAME_DATA_LEN_L ERROR value:%x", sg_data_len, value);
        sg_data_len = 0;
        sg_recv_data_state = FRAME_IDLE;
      } else {
        sg_frame_buf[5] = value;
        sg_frame_len = 6;
        sg_recv_data_state = FRAME_DATA_BYTES;
      }
      break;
    case FRAME_DATA_BYTES:
      sg_data_len -= 1;
      sg_frame_buf[sg_frame_len++] = value;
      if (sg_data_len <= 0) {
        sg_recv_data_state = FRAME_DATA_CRC;
      }
      break;
    case FRAME_DATA_CRC:
      sg_frame_buf[sg_frame_len++] = value;
      sg_recv_data_state = FRAME_TAIL1;
      break;
    case FRAME_TAIL1:
      if (FRAME_TAIL1_VALUE == value) {
        sg_recv_data_state = FRAME_TAIL2;
      } else {
        sg_recv_data_state = FRAME_IDLE;
        sg_frame_len = 0;
        sg_data_len = 0;
        ESP_LOGD(TAG, "FRAME_TAIL1 ERROR value:%x", value);
      }
      break;
    case FRAME_TAIL2:
      if (FRAME_TAIL2_VALUE == value) {
        sg_frame_buf[sg_frame_len++] = FRAME_TAIL1_VALUE;
        sg_frame_buf[sg_frame_len++] = FRAME_TAIL2_VALUE;
        memcpy(sg_frame_prase_buf, sg_frame_buf, sg_frame_len);
        if (get_frame_check_status(sg_frame_prase_buf, sg_frame_len)) {
          this->r24_parse_data_frame(sg_frame_prase_buf, sg_frame_len);
        } else {
          ESP_LOGD(TAG, "frame check failer!");
        }
      } else {
        ESP_LOGD(TAG, "FRAME_TAIL2 ERROR value:%x", value);
      }
      memset(sg_frame_prase_buf, 0, FRAME_BUF_MAX_SIZE);
      memset(sg_frame_buf, 0, FRAME_BUF_MAX_SIZE);
      sg_frame_len = 0;
      sg_data_len = 0;
      sg_recv_data_state = FRAME_IDLE;
      break;
    default:
      sg_recv_data_state = FRAME_IDLE;
  }
}

// Parses data frames related to product information
void MR24HPC1Component::r24_frame_parse_product_information(uint8_t *data) {
  uint8_t product_len = 0;
  if (data[FRAME_COMMAND_WORD_INDEX] == 0xA1) {
    product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
    if (product_len < PRODUCT_BUF_MAX_SIZE) {
      memset(this->c_product_mode_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_product_mode_, &data[FRAME_DATA_INDEX], product_len);
      this->product_model_text_sensor_->publish_state(this->c_product_mode_);
    } else {
      ESP_LOGD(TAG, "Reply: get product_mode length too long!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA2) {
    product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
    if (product_len < PRODUCT_BUF_MAX_SIZE) {
      memset(this->c_product_id_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_product_id_, &data[FRAME_DATA_INDEX], product_len);
      this->product_id_text_sensor_->publish_state(this->c_product_id_);
    } else {
      ESP_LOGD(TAG, "Reply: get productId length too long!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA3) {
    product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
    if (product_len < PRODUCT_BUF_MAX_SIZE) {
      memset(this->c_hardware_model_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_hardware_model_, &data[FRAME_DATA_INDEX], product_len);
      this->hardware_model_text_sensor_->publish_state(this->c_hardware_model_);
      ESP_LOGD(TAG, "Reply: get hardware_model :%s", this->c_hardware_model_);
    } else {
      ESP_LOGD(TAG, "Reply: get hardwareModel length too long!");
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA4) {
    product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
    if (product_len < PRODUCT_BUF_MAX_SIZE) {
      memset(this->c_firmware_version_, 0, PRODUCT_BUF_MAX_SIZE);
      memcpy(this->c_firmware_version_, &data[FRAME_DATA_INDEX], product_len);
      this->firware_version_text_sensor_->publish_state(this->c_firmware_version_);
    } else {
      ESP_LOGD(TAG, "Reply: get firmwareVersion length too long!");
    }
  }
}

// Parsing the underlying open parameters
void MR24HPC1Component::r24_frame_parse_open_underlying_information(uint8_t *data) {
  if (data[FRAME_COMMAND_WORD_INDEX] == 0x00) {
    this->underly_open_function_switch_->publish_state(
        data[FRAME_DATA_INDEX]);  // Underlying Open Parameter Switch Status Updates
    if (data[FRAME_DATA_INDEX]) {
      s_output_info_switch_flag = OUTPUT_SWTICH_ON;
    } else {
      s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
    this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    this->custom_presence_of_detection_sensor_->publish_state(data[FRAME_DATA_INDEX + 1] * 0.5f);
    this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX + 2]);
    this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX + 3] * 0.5f);
    this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX + 4] - 10) * 0.5f);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x06) {
    // none:0x00  close_to:0x01  far_away:0x02
    if (data[FRAME_DATA_INDEX] < 3) {
      this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07) {
    this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x08) {
    this->existence_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09) {
    this->motion_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0a) {
    if (this->existence_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->existence_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0b) {
    if (this->motion_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->motion_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0c) {
    uint32_t motion_trigger_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    this->motion_trigger_number_->publish_state(motion_trigger_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0d) {
    uint32_t move_to_rest_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                 (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                 (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    this->motion_to_rest_number_->publish_state(move_to_rest_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0e) {
    uint32_t enter_unmanned_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    float custom_unmanned_time = enter_unmanned_time / 1000;
    this->custom_unman_time_number_->publish_state(custom_unmanned_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x80) {
    if (data[FRAME_DATA_INDEX]) {
      s_output_info_switch_flag = OUTPUT_SWTICH_ON;
    } else {
      s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
    }
    this->underly_open_function_switch_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
    this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82) {
    this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83) {
    this->custom_presence_of_detection_sensor_->publish_state(
        S_PRESENCE_OF_DETECTION_RANGE_STR[data[FRAME_DATA_INDEX]]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x84) {
    this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX] * 0.5f);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x85) {
    this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX] - 10) * 0.5f);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x86) {
    this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87) {
    this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x88) {
    this->existence_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89) {
    this->motion_threshold_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8a) {
    if (this->existence_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->existence_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8b) {
    if (this->motion_boundary_select_->has_index(data[FRAME_DATA_INDEX] - 1)) {
      this->motion_boundary_select_->publish_state(S_BOUNDARY_STR[data[FRAME_DATA_INDEX] - 1]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8c) {
    uint32_t motion_trigger_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    this->motion_trigger_number_->publish_state(motion_trigger_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8d) {
    uint32_t move_to_rest_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                 (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                 (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    this->motion_to_rest_number_->publish_state(move_to_rest_time);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8e) {
    uint32_t enter_unmanned_time = (uint32_t) (data[FRAME_DATA_INDEX] << 24) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 1] << 16) +
                                   (uint32_t) (data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
    float custom_unmanned_time = enter_unmanned_time / 1000;
    this->custom_unman_time_number_->publish_state(custom_unmanned_time);
  }
}

void MR24HPC1Component::r24_parse_data_frame(uint8_t *data, uint8_t len) {
  switch (data[FRAME_CONTROL_WORD_INDEX]) {
    case 0x01: {
      if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
        this->heartbeat_state_text_sensor_->publish_state("Equipment Normal");
      } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02) {
        ESP_LOGD(TAG, "Reply: query reset packet");
      } else
        this->heartbeat_state_text_sensor_->publish_state("Equipment Abnormal");
    } break;
    case 0x02: {
      this->r24_frame_parse_product_information(data);
    } break;
    case 0x05: {
      this->r24_frame_parse_work_status(data);
    } break;
    case 0x08: {
      this->r24_frame_parse_open_underlying_information(data);
    } break;
    case 0x80: {
      this->r24_frame_parse_human_information(data);
    } break;
    default:
      ESP_LOGD(TAG, "control world:0x%02X not found", data[FRAME_CONTROL_WORD_INDEX]);
      break;
  }
}

void MR24HPC1Component::r24_frame_parse_work_status(uint8_t *data) {
  if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
    ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07) {
    if (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX])) {
      this->scene_mode_select_->publish_state(S_SCENE_STR[data[FRAME_DATA_INDEX]]);
    } else {
      ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x08) {
    // 1-3
    this->sensitivity_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09) {
    // 1-4
    this->custom_mode_num_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    this->custom_mode_number_->publish_state(0);
    this->custom_mode_end_text_sensor_->publish_state("Setup in progress...");
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
    ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87) {
    if (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX])) {
      this->scene_mode_select_->publish_state(S_SCENE_STR[data[FRAME_DATA_INDEX]]);
    } else {
      ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x88) {
    this->sensitivity_number_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0A) {
    this->custom_mode_end_text_sensor_->publish_state("Set Success!");
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89) {
    if (data[FRAME_DATA_INDEX] == 0)
      this->custom_mode_end_text_sensor_->publish_state("Not in custom mode");
    this->custom_mode_number_->publish_state(0);
    this->custom_mode_num_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else {
    ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
  }
}

void MR24HPC1Component::r24_frame_parse_human_information(uint8_t *data) {
  if (data[FRAME_COMMAND_WORD_INDEX] == 0x01) {
    this->someoneExists_binary_sensor_->publish_state(S_SOMEONE_EXISTS_STR[data[FRAME_DATA_INDEX]]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02) {
    if (data[FRAME_DATA_INDEX] < 3) {
      this->motion_status_text_sensor_->publish_state(S_MOTION_STATUS_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x03) {
    this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0A) {
    // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
    if (data[FRAME_DATA_INDEX] < 9) {
      this->unman_time_select_->publish_state(S_UNMANNED_TIME_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0B) {
    // none:0x00  close_to:0x01  far_away:0x02
    if (data[FRAME_DATA_INDEX] < 3) {
      this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
    this->someoneExists_binary_sensor_->publish_state(S_SOMEONE_EXISTS_STR[data[FRAME_DATA_INDEX]]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82) {
    if (data[FRAME_DATA_INDEX] < 3) {
      this->motion_status_text_sensor_->publish_state(S_MOTION_STATUS_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83) {
    this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8A) {
    // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
    if (data[FRAME_DATA_INDEX] < 9) {
      this->unman_time_select_->publish_state(S_UNMANNED_TIME_STR[data[FRAME_DATA_INDEX]]);
    }
  } else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8B) {
    // none:0x00  close_to:0x01  far_away:0x02
    if (data[FRAME_DATA_INDEX] < 3) {
      this->keep_away_text_sensor_->publish_state(S_KEEP_AWAY_STR[data[FRAME_DATA_INDEX]]);
    }
  } else {
    ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
  }
}

// Print data frame
static void show_frame_data(const uint8_t *data, int len) {
  printf("[%s] FRAME: %d, ", __FUNCTION__, len);
  for (int i = 0; i < len; i++) {
    printf("%02X ", data[i] & 0xff);
  }
  printf("\r\n");
}

// Sending data frames
void MR24HPC1Component::send_query(uint8_t *query, size_t string_length) {
  int i;
  for (i = 0; i < string_length; i++) {
    write(query[i]);
  }
  show_frame_data(query, i);
}

// Send Heartbeat Packet Command
void MR24HPC1Component::get_heartbeat_packet() {
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x01, 0x01, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Issuance of the underlying open parameter query command
void MR24HPC1Component::get_radar_output_information_switch() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x80, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Issuance of product model orders
void MR24HPC1Component::get_product_mode() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA1, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Issuing the Get Product ID command
void MR24HPC1Component::get_product_id() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA2, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Issuing hardware model commands
void MR24HPC1Component::get_hardware_model() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA3, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Issuing software version commands
void MR24HPC1Component::get_firmware_version() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA4, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_human_status() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x81, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_human_motion_info() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x82, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_body_motion_params() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x83, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_keep_away() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x8B, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_scene_mode() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x05, 0x87, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_sensitivity() {
  unsigned int send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x05, 0x88, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_unmanned_time() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x8a, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_custom_mode() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x05, 0x89, 0x00, 0x01, 0x0F, 0x4A, 0x54, 0x43};
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_existence_boundary() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x8A, 0x00, 0x01, 0x0F, 0x45, 0x54, 0x43};
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_motion_boundary() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x8B, 0x00, 0x01, 0x0F, 0x46, 0x54, 0x43};
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_spatial_static_value() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x81, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_spatial_motion_value() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x82, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_distance_of_static_object() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x83, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_distance_of_moving_object() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x84, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_target_movement_speed() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x85, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_existence_threshold() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x88, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_motion_threshold() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x89, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_motion_trigger_time() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x8C, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_motion_to_rest_time() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x8D, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

void MR24HPC1Component::get_custom_unman_time() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x8E, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
}

// Logic of setting: After setting, query whether the setting is successful or not!

void MR24HPC1Component::set_underlying_open_function(bool enable) {
  uint8_t underlyswitch_on[] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x01, 0xB6, 0x54, 0x43};
  uint8_t underlyswitch_off[] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x00, 0xB5, 0x54, 0x43};
  if (enable) {
    send_query(underlyswitch_on, sizeof(underlyswitch_on));
  } else {
    send_query(underlyswitch_off, sizeof(underlyswitch_off));
  }
  this->keep_away_text_sensor_->publish_state("");
  this->motion_status_text_sensor_->publish_state("");
  this->custom_spatial_static_value_sensor_->publish_state(0.0f);
  this->custom_spatial_motion_value_sensor_->publish_state(0.0f);
  this->custom_motion_distance_sensor_->publish_state(0.0f);
  this->custom_presence_of_detection_sensor_->publish_state(0.0f);
  this->custom_motion_speed_sensor_->publish_state(0.0f);
}

void MR24HPC1Component::set_scene_mode(const std::string &state) {
  uint8_t cmd_value = SCENEMODE_ENUM_TO_INT.at(state);
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x07, 0x00, 0x01, cmd_value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->custom_mode_number_->publish_state(0);
  this->custom_mode_num_sensor_->publish_state(0);
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
  this->send_query(send_data, send_data_len);
  this->get_scene_mode();
  this->get_sensitivity();
}

void MR24HPC1Component::set_reset() {
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x01, 0x02, 0x00, 0x01, 0x0F, 0xBF, 0x54, 0x43};
  this->send_query(send_data, send_data_len);
  check_dev_inf_sign = true;
}

void MR24HPC1Component::set_unman_time(const std::string &time) {
  uint8_t cmd_value = UNMANDTIME_ENUM_TO_INT.at(time);
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x80, 0x0a, 0x00, 0x01, cmd_value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_unmanned_time();
}

void MR24HPC1Component::set_custom_mode(uint8_t mode) {
  if (mode == 0)
    this->set_custom_end_mode();  // Equivalent to end setting
  this->custom_mode_number_->publish_state(0);
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x05, 0x09, 0x00, 0x01, mode, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
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
  this->send_query(send_data, send_data_len);
  this->custom_mode_number_->publish_state(0);  // Clear setpoints
  this->custom_mode_num_sensor_->publish_state(0);
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

void MR24HPC1Component::set_existence_boundary(const std::string &value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t cmd_value = BOUNDARY_ENUM_TO_INT.at(value);
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x0A, 0x00, 0x01, cmd_value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_existence_boundary();
}

void MR24HPC1Component::set_motion_boundary(const std::string &value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  uint8_t cmd_value = BOUNDARY_ENUM_TO_INT.at(value);
  uint8_t send_data_len = 10;
  uint8_t send_data[10] = {0x53, 0x59, 0x08, 0x0B, 0x00, 0x01, cmd_value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_motion_boundary();
}

void MR24HPC1Component::set_existence_threshold(int value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x08, 0x00, 0x01, (uint8_t) value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_existence_threshold();
}

void MR24HPC1Component::set_motion_threshold(int value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  unsigned char send_data_len = 10;
  unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x09, 0x00, 0x01, (uint8_t) value, 0x00, 0x54, 0x43};
  send_data[7] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_motion_threshold();
}

void MR24HPC1Component::set_motion_trigger_time(int value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  int h24_num = (value >> 24) & 0xff;
  int h16_num = (value >> 16) & 0xff;
  int h8_num = (value >> 8) & 0xff;
  int l8_num = value & 0xff;
  unsigned char send_data_len = 13;
  unsigned char send_data[13] = {
      0x53, 0x59, 0x08, 0x0C, 0x00, 0x04, (uint8_t) h24_num, (uint8_t) h16_num, (uint8_t) h8_num, (uint8_t) l8_num,
      0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_motion_trigger_time();
}

void MR24HPC1Component::set_motion_to_rest_time(int value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  int h24_num = (value >> 24) & 0xff;
  int h16_num = (value >> 16) & 0xff;
  int h8_num = (value >> 8) & 0xff;
  int l8_num = value & 0xff;
  unsigned char send_data_len = 13;
  unsigned char send_data[13] = {
      0x53, 0x59, 0x08, 0x0D, 0x00, 0x04, (uint8_t) h24_num, (uint8_t) h16_num, (uint8_t) h8_num, (uint8_t) l8_num,
      0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_motion_to_rest_time();
}

void MR24HPC1Component::set_custom_unman_time(int value) {
  if (this->custom_mode_num_sensor_->state == 0)
    return;  // You'll have to check that you're in custom mode to set it up
  value *= 1000;
  int h24_num = (value >> 24) & 0xff;
  int h16_num = (value >> 16) & 0xff;
  int h8_num = (value >> 8) & 0xff;
  int l8_num = value & 0xff;
  unsigned char send_data_len = 13;
  unsigned char send_data[13] = {
      0x53, 0x59, 0x08, 0x0E, 0x00, 0x04, (uint8_t) h24_num, (uint8_t) h16_num, (uint8_t) h8_num, (uint8_t) l8_num,
      0x00, 0x54, 0x43};
  send_data[10] = get_frame_crc_sum(send_data, send_data_len);
  this->send_query(send_data, send_data_len);
  this->get_custom_unman_time();
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
