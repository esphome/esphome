#pragma once
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "seeed_mr24hpc1_constants.h"

#include <map>

namespace esphome {
namespace seeed_mr24hpc1 {

enum FrameState {
  FRAME_IDLE,
  FRAME_HEADER2,
  FRAME_CTL_WORD,
  FRAME_CMD_WORD,
  FRAME_DATA_LEN_H,
  FRAME_DATA_LEN_L,
  FRAME_DATA_BYTES,
  FRAME_DATA_CRC,
  FRAME_TAIL1,
  FRAME_TAIL2,
};

enum PollingState {
  STANDARD_FUNCTION_QUERY_PRODUCT_MODE = 0,
  STANDARD_FUNCTION_QUERY_PRODUCT_ID,
  STANDARD_FUNCTION_QUERY_FIRMWARE_VERSION,
  STANDARD_FUNCTION_QUERY_HARDWARE_MODE,  // Above is the equipment information
  STANDARD_FUNCTION_QUERY_SCENE_MODE,
  STANDARD_FUNCTION_QUERY_SENSITIVITY,
  STANDARD_FUNCTION_QUERY_UNMANNED_TIME,
  STANDARD_FUNCTION_QUERY_HUMAN_STATUS,
  STANDARD_FUNCTION_QUERY_HUMAN_MOTION_INF,
  STANDARD_FUNCTION_QUERY_BODY_MOVE_PARAMETER,
  STANDARD_FUNCTION_QUERY_KEEPAWAY_STATUS,
  STANDARD_QUERY_CUSTOM_MODE,
  STANDARD_FUNCTION_QUERY_HEARTBEAT_STATE,  // Above is the basic function

  CUSTOM_FUNCTION_QUERY_EXISTENCE_BOUNDARY,
  CUSTOM_FUNCTION_QUERY_MOTION_BOUNDARY,
  CUSTOM_FUNCTION_QUERY_EXISTENCE_THRESHOLD,
  CUSTOM_FUNCTION_QUERY_MOTION_THRESHOLD,
  CUSTOM_FUNCTION_QUERY_MOTION_TRIGGER_TIME,
  CUSTOM_FUNCTION_QUERY_MOTION_TO_REST_TIME,
  CUSTOM_FUNCTION_QUERY_TIME_OF_ENTER_UNMANNED,

  UNDERLY_FUNCTION_QUERY_HUMAN_STATUS,
  UNDERLY_FUNCTION_QUERY_SPATIAL_STATIC_VALUE,
  UNDERLY_FUNCTION_QUERY_SPATIAL_MOTION_VALUE,
  UNDERLY_FUNCTION_QUERY_DISTANCE_OF_STATIC_OBJECT,
  UNDERLY_FUNCTION_QUERY_DISTANCE_OF_MOVING_OBJECT,
  UNDERLY_FUNCTION_QUERY_TARGET_MOVEMENT_SPEED,
};

enum OutputSwitch {
  OUTPUT_SWITCH_INIT,
  OUTPUT_SWITCH_ON,
  OUTPUT_SWTICH_OFF,
};

static const char *const S_SCENE_STR[5] = {"None", "Living Room", "Bedroom", "Washroom", "Area Detection"};
static const bool S_SOMEONE_EXISTS_STR[2] = {false, true};
static const char *const S_MOTION_STATUS_STR[3] = {"None", "Motionless", "Active"};
static const char *const S_KEEP_AWAY_STR[3] = {"None", "Close", "Away"};
static const char *const S_UNMANNED_TIME_STR[9] = {"None", "10s",   "30s",   "1min", "2min",
                                                   "5min", "10min", "30min", "60min"};
static const char *const S_BOUNDARY_STR[10] = {"0.5m", "1.0m", "1.5m", "2.0m", "2.5m",
                                               "3.0m", "3.5m", "4.0m", "4.5m", "5.0m"};                // uint: m
static const float S_PRESENCE_OF_DETECTION_RANGE_STR[7] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};  // uint: m

class MR24HPC1Component : public Component,
                          public uart::UARTDevice {  // The class name must be the name defined by text_sensor.py
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(heartbeat_state)
  SUB_TEXT_SENSOR(product_model)
  SUB_TEXT_SENSOR(product_id)
  SUB_TEXT_SENSOR(hardware_model)
  SUB_TEXT_SENSOR(firware_version)
  SUB_TEXT_SENSOR(keep_away)
  SUB_TEXT_SENSOR(motion_status)
  SUB_TEXT_SENSOR(custom_mode_end)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(has_target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(custom_presence_of_detection)
  SUB_SENSOR(movement_signs)
  SUB_SENSOR(custom_motion_distance)
  SUB_SENSOR(custom_spatial_static_value)
  SUB_SENSOR(custom_spatial_motion_value)
  SUB_SENSOR(custom_motion_speed)
  SUB_SENSOR(custom_mode_num)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(underlying_open_function)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(restart)
  SUB_BUTTON(custom_set_end)
#endif
#ifdef USE_SELECT
  SUB_SELECT(scene_mode)
  SUB_SELECT(unman_time)
  SUB_SELECT(existence_boundary)
  SUB_SELECT(motion_boundary)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(sensitivity)
  SUB_NUMBER(custom_mode)
  SUB_NUMBER(existence_threshold)
  SUB_NUMBER(motion_threshold)
  SUB_NUMBER(motion_trigger)
  SUB_NUMBER(motion_to_rest)
  SUB_NUMBER(custom_unman_time)
#endif

 protected:
  char c_product_mode_[PRODUCT_BUF_MAX_SIZE + 1];
  char c_product_id_[PRODUCT_BUF_MAX_SIZE + 1];
  char c_hardware_model_[PRODUCT_BUF_MAX_SIZE + 1];
  char c_firmware_version_[PRODUCT_BUF_MAX_SIZE + 1];
  uint8_t s_output_info_switch_flag_;
  uint8_t sg_recv_data_state_;
  uint8_t sg_frame_len_;
  uint8_t sg_data_len_;
  uint8_t sg_frame_buf_[FRAME_BUF_MAX_SIZE];
  uint8_t sg_frame_prase_buf_[FRAME_BUF_MAX_SIZE];
  int sg_start_query_data_;
  bool check_dev_inf_sign_;
  bool poll_time_base_func_check_;

  void update_();
  void r24_split_data_frame_(uint8_t value);
  void r24_parse_data_frame_(uint8_t *data, uint8_t len);
  void r24_frame_parse_open_underlying_information_(uint8_t *data);
  void r24_frame_parse_work_status_(uint8_t *data);
  void r24_frame_parse_product_information_(uint8_t *data);
  void r24_frame_parse_human_information_(uint8_t *data);
  void send_query_(const uint8_t *query, size_t string_length);

 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

  void get_heartbeat_packet();
  void get_radar_output_information_switch();
  void get_product_mode();
  void get_product_id();
  void get_hardware_model();
  void get_firmware_version();
  void get_human_status();
  void get_human_motion_info();
  void get_body_motion_params();
  void get_keep_away();
  void get_scene_mode();
  void get_sensitivity();
  void get_unmanned_time();
  void get_custom_mode();
  void get_existence_boundary();
  void get_motion_boundary();
  void get_spatial_static_value();
  void get_spatial_motion_value();
  void get_distance_of_static_object();
  void get_distance_of_moving_object();
  void get_target_movement_speed();
  void get_existence_threshold();
  void get_motion_threshold();
  void get_motion_trigger_time();
  void get_motion_to_rest_time();
  void get_custom_unman_time();

  void set_scene_mode(uint8_t value);
  void set_underlying_open_function(bool enable);
  void set_sensitivity(uint8_t value);
  void set_restart();
  void set_unman_time(uint8_t value);
  void set_custom_mode(uint8_t mode);
  void set_custom_end_mode();
  void set_existence_boundary(uint8_t value);
  void set_motion_boundary(uint8_t value);
  void set_existence_threshold(uint8_t value);
  void set_motion_threshold(uint8_t value);
  void set_motion_trigger_time(uint8_t value);
  void set_motion_to_rest_time(uint16_t value);
  void set_custom_unman_time(uint16_t value);
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome
