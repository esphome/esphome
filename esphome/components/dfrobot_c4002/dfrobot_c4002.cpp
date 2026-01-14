#include "dfrobot_c4002.h"
#include <string>
#include <cstdio>

namespace esphome {
namespace dfrobot_c4002 {

static const char *const TAG = "dfrobot_c4002: ";

/**
 * setup
 * Called once when the component is initialized.
 * We call update_config_param() to load device configuration and publish initial values.
 */
void C4002Component::setup() {
  update_config_param();
  this->publish_text("The initialization of c4002 was successful!");
}

/**
 * print_config
 * Print current configuration values to the log for debugging.
 */
void C4002Component::print_config() { ESP_LOGD(TAG, "run print config"); }

/**
 * loop
 * Main periodic loop called frequently by ESPHome.
 * We call get_data() every 1000 ms to read and parse UART data.
 */
void C4002Component::loop() {
  // Perform periodic tasks here
  static uint32_t last_time = 0;
  uint32_t now = millis();
  RetResult ret = {};

  ret = get_note_info_loop();
  if (ret.noteType == NOTE_INFO_RESULT) {
    ESP_LOGD(TAG, "******run print NOTE_INFO_RESULT*********");
  } else if (ret.noteType == NOTE_INFO_CALIBRATION) {
    ESP_LOGD(TAG, "********Calibration countdown: %2d s**********", ret.calibCountdown);
    if (ret.calibCountdown == 0) {
      ESP_LOGD(TAG, "Calibration complete!");
      this->publish_text("Calibration complete!");
      analysis_text_report();
    }
  }

  if (now - last_time >= 1000) {  // Execute every 1000ms
    last_time = now;
    get_data();
  }

  if (reset_flag_ == 1) {
    reset_flag_ = 0;
    restart();
    this->publish_text("Reset the device to factory complete!");
  }
}

/**
 * get_data
 * Read UART and update internal state depending on current run mode.
 * - In MODE_MOTION: parse $DFHPD for exist flag.
 * - In MODE_SPEED: clear buffer and parse $DFDMD for exist/distance/speed.
 *
 * After parsing, update exist_, speed_, distance_ members.
 */
void C4002Component::get_data() {
  ExistTgt exit_taget_data = get_exist_target_info();
  MoveTgt move_taget_data = get_move_target_info();
  TargetState target_state = get_target_state();

  for (auto &listener : this->listeners_) {
    if (listener != nullptr) {
      listener->on_movement_distance(move_taget_data.distance);
      listener->on_movement_speed(move_taget_data.speed);
      listener->on_movement_direction(static_cast<float>(move_taget_data.direction));
      listener->on_existing_distance(exit_taget_data.distance);
      listener->on_target_status((uint8_t) target_state);
    }
  }
}

/**
 * update_config_param for setup
 * Query device settings and update all configured number entities and other state.
 * This is typically called on setup or when settings change.
 */
void C4002Component::update_config_param() {
  ESP_LOGD(TAG, "update config param test!");

  //** driver init **/
  while (!begin()) {
    delayMicroseconds(1000 * 300);

    ESP_LOGD(TAG, "C4002 begin failed");
  }
  ESP_LOGD(TAG, "C4002 begin success");

  setup_number();

  //** read config param **//
  float current_light_threshold = get_light_threshold();
  float current_delay_time = get_target_disappear_delay();

  if (min_range_number_ != nullptr) {
    min_range_number_->publish_state(current_detection_range_min_);
    ESP_LOGD(TAG, "Publishing min_range_: %.2f", current_detection_range_min_);
  }
  if (max_range_number_ != nullptr) {
    max_range_number_->publish_state(current_detection_range_max_);
    ESP_LOGD(TAG, "Publishing max_range_: %.2f", current_detection_range_max_);
  }
  if (light_threshold_number_ != nullptr) {
    light_threshold_number_->publish_state(current_light_threshold);
    ESP_LOGD(TAG, "Publishing light_threshold_: %.2f", current_light_threshold);
  }

  for (float &v : current_area_) {
    v = 0.0f;
  }

  joint_enable_door();

  if (area1_min_range_number_ != nullptr) {
    area1_min_range_number_->publish_state(current_area_[AREA1_DOOR_MIN]);
  }
  if (area2_min_range_number_ != nullptr) {
    area2_min_range_number_->publish_state(current_area_[AREA2_DOOR_MIN]);
  }
  if (area3_min_range_number_ != nullptr) {
    area3_min_range_number_->publish_state(current_area_[AREA3_DOOR_MIN]);
  }
  if (area1_max_range_number_ != nullptr) {
    area1_max_range_number_->publish_state(current_area_[AREA1_DOOR_MAX]);
  }
  if (area2_max_range_number_ != nullptr) {
    area2_max_range_number_->publish_state(current_area_[AREA2_DOOR_MAX]);
  }
  if (area3_max_range_number_ != nullptr) {
    area3_max_range_number_->publish_state(current_area_[AREA3_DOOR_MAX]);
  }

  if (target_disappeard_delay_time_number_ != nullptr) {
    target_disappeard_delay_time_number_->publish_state(current_delay_time);
  }

  if (run_led_switch_ != nullptr) {
    set_run_led(LED_ON);
    run_led_switch_->publish_state((bool) LED_ON);
  }
  if (out_led_switch_ != nullptr) {
    set_out_led(LED_ON);
    out_led_switch_->publish_state((bool) LED_ON);
  }

  //** config report period **//
  if (set_report_period(10)) {
    ESP_LOGD(TAG, "set report period success");
  } else {
    ESP_LOGD(TAG, "set report period failed");
  }
}

/**
 * get_out_mode
 * Get the output mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::get_out_mode() {
  uint8_t send_date[10];
  uint16_t data_len = 4;
  send_date[0] = CMD_CONFIG_OUT_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    out_mode_ = (OutMode) rec_pack.data[0];
    return true;
  } else {
    return false;
  }
}

/**
 * set_out_mode
 * Set the output mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_out_mode(OutMode out_mode) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 5;
  send_date[data_len++] = CMD_CONFIG_OUT_MODE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = (uint8_t) out_mode;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    out_mode_ = out_mode;
    return true;
  } else {
    return false;
  }
}

/**
 * set_light_threshold
 * Set the light threshold of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_light_threshold(float threshold) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 6;
  send_date[data_len++] = CMD_SET_LIGHT_THRESHOLD;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  uint16_t threshold_temp = (uint16_t) (threshold * 10);
  send_date[data_len++] = threshold_temp >> 0 & 0xFF;
  send_date[data_len++] = threshold_temp >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * factory_reset
 * Reset the device to factory default settings.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::factory_reset() {
  uint8_t send_date[10];
  uint16_t data_len = 5;

  send_date[0] = CMD_FACTORY_RESET;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_date[4] = 0x00;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED != rec_pack.resPonCode) {
    return false;
  }
  delay(10);

  send_date[0] = CMD_FACTORY_RESET_USER;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);
  rec_pack = recv_pack();
  if (SUCCEED != rec_pack.resPonCode) {
    return false;
  }
  reset_flag_ = 1;

  return true;
}

/**
 * set_resolution_mode
 * Set the resolution mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_resolution_mode(ResolutionMode mode) {
  uint8_t send_date[10];
  uint16_t data_len = 5;
  send_date[0] = CMD_GET_AND_SET_RESOLUTION_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_date[4] = (uint8_t) mode;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    resolution_mode_ = mode;
    return true;
  } else {
    return false;
  }
}

/**
 * enable_distance_door
 * Enable the distance door.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::enable_distance_door(DistanceDoorType door_type, const uint8_t *door_data) {
  uint8_t send_date[40];
  uint16_t data_len = 0;
  uint16_t temp = 5;
  int door_num = 0;
  if (resolution_mode_ == RESOLUTION_80CM) {
    door_num = 15;
  } else if (resolution_mode_ == RESOLUTION_20CM) {
    door_num = 25;
  }
  temp += door_num;

  send_date[data_len++] = CMD_SET_DISTANCE_DOOR;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = (uint8_t) door_type;
  for (int i = 0; i < door_num; i++) {
    send_date[data_len++] = door_data[i];
  }
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * set_detect_range
 * Set the detection range of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_detect_range(uint16_t closest, uint16_t farthest)  // 0-1200cm
{
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 8;
  uint16_t closest_temp = closest, farthest_temp = farthest;
  send_date[data_len++] = CMD_SET_DETECT_RANGE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;

  if (farthest_temp > 1200) {
    farthest_temp = 1200;
  }
  if (closest_temp > farthest_temp) {
    return false;
  }
  send_date[data_len++] = closest_temp >> 0 & 0xFF;
  send_date[data_len++] = closest_temp >> 8 & 0xFF;
  send_date[data_len++] = farthest_temp >> 0 & 0xFF;
  send_date[data_len++] = farthest_temp >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();

  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * start_env_calibration
 * Start the environment calibration.
 */
void C4002Component::start_env_calibration(uint16_t delay_time, uint16_t cont_time) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 9;
  send_date[data_len++] = CMD_ENVIRNMENT_CALIBRATION;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = delay_time >> 0 & 0xFF;
  send_date[data_len++] = delay_time >> 8 & 0xFF;
  send_date[data_len++] = cont_time >> 0 & 0xFF;
  send_date[data_len++] = cont_time >> 8 & 0xFF;
  send_date[data_len++] = 0x01;  // Automatically generate thresholds
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  recv_pack();
}

/**
 * set_run_led
 * Set the state of the run LED.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_run_led(LedMode run_led) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 6;
  send_date[data_len++] = CMD_SET_LED_MODE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = run_led;
  send_date[data_len++] = LED_KEEP;

  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * set_out_led
 * Set the state of the output LED.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_out_led(LedMode out_led) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 6;

  send_date[data_len++] = CMD_SET_LED_MODE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = LED_KEEP;
  send_date[data_len++] = out_led;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * set_target_disappear_delay
 * Set the delay time of the target disappear.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_target_disappear_delay(uint16_t delay_time) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 6;

  send_date[data_len++] = CMD_TARGET_DISAPPEAR_DELAY_TIME;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = delay_time >> 0 & 0xFF;
  send_date[data_len++] = delay_time >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * get_target_disappear_delay
 * Get the delay time of the target disappear.
 * Returns the delay time in ms.
 */
uint16_t C4002Component::get_target_disappear_delay() {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 4;
  send_date[data_len++] = CMD_TARGET_DISAPPEAR_DELAY_TIME;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    uint16_t delay_time = (rec_pack.data[1] << 8) | rec_pack.data[0];
    return delay_time;
  }
  return 0;
}

/**
 * restart
 * Restart the device.
 */
int8_t C4002Component::restart() {
  int8_t ret = 0;
  uint8_t send_date[10];
  uint16_t data_len = 5;
  send_date[0] = CMD_RESTART;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_date[4] = 0x00;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    ret = 0;
  } else {
    ret = -1;
  }

  for (int i = 0; i < 50; i++) {
    delay(10);
  }
  update_config_param();
  delay(10);
  return ret;
}

/**
 * get_distance_presence_threshold
 * Get the distance presence threshold of the device.
 * Returns true if successful, false otherwise.
 */
void C4002Component::get_distance_presence_threshold(DistanceDoorType door_type, uint8_t *gate_data) {
  uint8_t send_data[10];
  uint16_t data_len = 0;
  uint16_t temp = 7;
  uint8_t door_num = 15;
  uint8_t i = 0;

  send_data[data_len++] = CMD_SET_DISTANCE_DOOR_THRESHOLD;
  send_data[data_len++] = READ_AND_WRITE_REQ;
  send_data[data_len++] = temp >> 0 & 0xFF;
  send_data[data_len++] = temp >> 8 & 0xFF;
  send_data[data_len++] = door_type;
  send_data[data_len++] = 0xff;
  send_data[data_len++] = 0x00;

  RecvPack rec_pack;
  rec_pack.resPonCode = CMD_ERR;

  while (SUCCEED != rec_pack.resPonCode) {
    send_pack(send_data, data_len, FRAME_TYPE_READ_REQUSET);
    rec_pack = recv_pack();
    if (SUCCEED == rec_pack.resPonCode) {
      memcpy(gate_data, &rec_pack.data[3], door_num);
      return;
    }
    if (i++ > 5) {
      return;
    }
    delay(20);
  }
}

/**
 * analysis gate data
 * Analysis the gate data,send the result.
 */
void C4002Component::analysis_text_report() {
  uint8_t move_data[15], exist_data[15];
  uint8_t thld = 80;
  std::vector<uint8_t> over_indices;
  uint8_t flag = 0;

  get_distance_presence_threshold(MOVE_DIST_DOOR, move_data);
  get_distance_presence_threshold(EXIST_DIST_DOOR, exist_data);

  for (int8_t i = 0; i < 15; i++) {
    if (move_data[i] < exist_data[i]) {
      move_data[i] = exist_data[i];
    }

    if (move_data[i] > thld) {
      over_indices.push_back(i);
    }

    if (move_data[i] > thld && flag != 1) {
      flag = 1;
    }
  }

  if (flag == 0) {
    this->publish_text("The calibration threshold is effective!");
    return;
  }

  char data_str[180];
  int offset = 0;

  offset += snprintf(data_str + offset, sizeof(data_str) - offset,
                     "Calibration failed:Interference is detected at a distance of ");

  for (size_t i = 0; i < over_indices.size(); i++) {
    uint8_t idx = over_indices[i];

    offset += snprintf(data_str + offset, sizeof(data_str) - offset, "%.1f%s", interval_point_[idx],
                       (i < over_indices.size() - 1) ? ", " : "");
  }

  snprintf(data_str + offset, sizeof(data_str) - offset,
           " m, Please clear all interference sources within this range and recalibrate.");

  this->publish_text(data_str);
}

/**
 * get_target_state
 * Get the state of the target.
 * Returns the state of the target.
 */
TargetState C4002Component::get_target_state() { return (TargetState) detect_result_.targetStatus; }

/**
 * get_light
 * Get the light threshold of the device.
 * Returns the light threshold in Lux.
 */
float C4002Component::get_light() { return ((float) detect_result_.light * 0.1); }

/**
 * get_exit_dist_index
 * Get the index of the exit distance.
 * Returns the index of the exit distance.
 */
uint32_t C4002Component::get_exist_dist_index() { return detect_result_.existDistIndex; }

/**
 * get_exist_target_info
 * Get the information of the existing target.
 * Returns a ExistTgt struct with the information.
 */
ExistTgt C4002Component::get_exist_target_info() {
  ExistTgt info;
  info.distance = ((float) detect_result_.existTargetDist * 0.01);
  info.energy = detect_result_.existTargetEnery;
  return info;
}

/**
 * get_move_target_info
 * Get the information of the moving target.
 * Returns a MoveTgt struct with the information.
 */
MoveTgt C4002Component::get_move_target_info() {
  MoveTgt info;
  info.distance = ((float) detect_result_.moveTargetDist * 0.01);
  info.energy = detect_result_.moveTargetEnery;
  info.speed = ((float) detect_result_.moveTargetSpeed * 0.01);
  info.direction = (MoveDirection) detect_result_.moveTargetDirect;
  return info;
}

/**
 * begin
 * Initialize the device
 * Returns true if successful, false otherwise.
 */
bool C4002Component::begin() {
  bool ret;

  ret = set_report_period(255);
  if (!ret) {
    return false;
  }
  delay(10);
  ret = set_resolution_mode(resolution_mode_);
  if (!ret) {
    return false;
  }
  ret = enable_all_distance_door(enable_door_);
  return ret;
}

/**
 * enable_distance_door
 * Enable or disable a distance door.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::enable_all_distance_door(uint8_t *door_data) {
  bool ret = false;
  ret = enable_distance_door(MOVE_DIST_DOOR, door_data);
  if (!ret) {
    return false;
  }
  ret = enable_distance_door(EXIST_DIST_DOOR, door_data);

  return ret;
}

/**
 * get_note_info_loop
 * Read UART and parse notification data.
 * Returns a RetResult struct with the parsed data.
 */
RetResult C4002Component::get_note_info_loop() {
  RetResult ret = {};
  RecvPack rec_data = {};
  rec_data = recv_pack();

  if (SUCCEED == rec_data.resPonCode) {
    if (rec_data.packType == FRAME_TYPE_NOTIFICATION) {  // note
      if (rec_data.dataHeader.cmd == NOTE_RESULT_CMD) {
        // memcpy(&this->detect_result_, rec_data.data, sizeof(DetectRet));
        this->detect_result_.targetStatus = rec_data.data[0];
        this->detect_result_.light = rec_data.data[2] << 8 | rec_data.data[1];
        this->detect_result_.existDistIndex =
            rec_data.data[6] << 24 | rec_data.data[5] << 16 | rec_data.data[4] << 8 | rec_data.data[3];
        this->detect_result_.existCountDown = rec_data.data[8] << 8 | rec_data.data[7];
        this->detect_result_.existTargetDist = rec_data.data[10] << 8 | rec_data.data[9];
        this->detect_result_.existTargetEnery = rec_data.data[11];
        this->detect_result_.moveTargetDist = rec_data.data[13] << 8 | rec_data.data[12];
        this->detect_result_.moveTargetSpeed = rec_data.data[15] << 8 | rec_data.data[14];
        this->detect_result_.moveTargetEnery = rec_data.data[16];
        this->detect_result_.moveTargetDirect = rec_data.data[17];
        ret.noteType = NOTE_INFO_RESULT;
      } else if (rec_data.dataHeader.cmd == NOTE_ENVIRNMENT_CALIBRATION_CMD) {
        ret.calibCountdown = rec_data.data[1] << 8 | rec_data.data[0];
        ret.noteType = NOTE_INFO_CALIBRATION;
      } else {
        ret.noteType = NO_NOTE;
      }
    } else {
      ret.noteType = NO_NOTE;
    }

  } else {
    ret.noteType = NO_NOTE;
  }
  return ret;
}

/**
 * get_resolution_mode
 * Get the resolution mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::get_resolution_mode() {
  uint8_t send_date[10];
  uint16_t data_len = 4;
  send_date[0] = CMD_GET_AND_SET_RESOLUTION_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    resolution_mode_ = (ResolutionMode) rec_pack.data[0];
    return true;
  } else {
    return false;
  }
}

/**
 * set_report_period
 * Set the report period of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::set_report_period(uint8_t period)  //范围0-255.单位100ms
{
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 5;
  send_date[data_len++] = CMD_SET_REPORT_PERIOD;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = period;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack rec_pack = recv_pack();
  return (SUCCEED == rec_pack.resPonCode);
}

/**
 * send_pack
 * Send a data frame to the UART.
 * type uint8_t *pdata: Data to send.
 * type uint16_t len: Length of data to send.
 * type uint8_t msg_type: Type of message to send.
 */
void C4002Component::send_pack(void *pdata, uint16_t len, uint8_t msg_type) {
  uint8_t send_date[50] = {0};

  uint16_t data_len = 0;
  uint16_t check_sums = 0;

  send_date[data_len++] = C4002_FRAME_HEADER1;
  send_date[data_len++] = C4002_FRAME_HEADER2;
  send_date[data_len++] = C4002_FRAME_HEADER3;
  send_date[data_len++] = C4002_FRAME_HEADER4;
  uint16_t temp = len + 10;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = 0x00;
  send_date[data_len++] = msg_type;
  memcpy(&send_date[data_len], pdata, len);
  data_len += len;
  check_sums = get_check_sum((uint8_t *) send_date, data_len);

  send_date[data_len++] = check_sums >> 0 & 0xFF;
  send_date[data_len++] = check_sums >> 8 & 0xFF;

  uart_write_data(send_date, (size_t) data_len);
}

/**
 * recv_pack
 * Read a data frame from the UART and parse it.
 * Returns a RecvPack struct with the parsed data.
 */
RecvPack C4002Component::recv_pack() {
  RecvPack recv_dat;
  memset(&recv_dat, 0, sizeof(recv_dat));

  std::vector<uint8_t> pdata(60, 0);

  size_t recv_len = uart_read_raw(pdata.data(), 8, 20);

  if (recv_len == 8 && pdata[0] == C4002_FRAME_HEADER1 && pdata[1] == C4002_FRAME_HEADER2 &&
      pdata[2] == C4002_FRAME_HEADER3 && pdata[3] == C4002_FRAME_HEADER4) {
    size_t pack_len = (pdata[5] << 8) | pdata[4];

    recv_len = uart_read_raw(&pdata[8], (size_t) (pack_len - 8), 20);

    if (recv_len == (pack_len - 8)) {
      recv_dat.packType = pdata[7];
      if (check_sum(pdata.data(), pack_len)) {
        uint16_t data_len = (pdata[11] << 8) | pdata[10];

        memcpy(&recv_dat, &pdata[8], data_len);
        recv_dat.resPonCode = (ResponseCode) recv_dat.dataHeader.respCode;

        if (recv_dat.packType == FRAME_TYPE_NOTIFICATION) {
          ESP_LOGD(TAG, "get note result");
        } else if (recv_dat.packType == FRAME_TYPE_WRITE_RESPOND) {
          ESP_LOGD(TAG, "get write respond");
        } else if (recv_dat.packType == FRAME_TYPE_READ_RESPOND) {
          ESP_LOGD(TAG, "get read respond");
        } else {
          ESP_LOGD(TAG, "this is error pack");
          recv_dat.resPonCode = CMD_ERR;
        }
      } else {
        recv_dat.resPonCode = AUTHENTICATION_ERR;
        ESP_LOGD(TAG, "Authentication error");
      }
    } else {
      recv_dat.resPonCode = DATALEN_ERR;
      ESP_LOGD(TAG, " recvlen error");
    }
  } else {
    recv_dat.resPonCode = AUTHENTICATION_ERR;
  }
  return recv_dat;
}

/**
 * check_sum
 * Check the check_sum of the data.
 */
bool C4002Component::check_sum(const uint8_t *pdata, uint8_t len) {
  uint16_t calculateparity = 0;

  for (uint8_t i = 0; i < len - 2; i++) {
    calculateparity += pdata[i];
  }
  uint16_t temp = (pdata[len - 1] << 8) | pdata[len - 2];
  return (calculateparity == temp);
}

/**
 * get_check_sum
 * Calculate the checksum of the data.
 */
uint16_t C4002Component::get_check_sum(const uint8_t *pdata, uint16_t len) {
  uint16_t parity = 0;
  for (uint16_t i = 0; i < len; i++) {
    parity += pdata[i];
  }
  return parity;
}

/**
 * uart_clear_buffer
 *t Drain and discard any pending bytes from the UART RX buffer.
 * Useful to ensure subsequent read returns fresh data.
 */
void C4002Component::uart_clear_buffer() {
  uint8_t tmp[64];  // Temporary buffer
  while (this->available() > 0) {
    size_t toread = std::min(static_cast<size_t>(this->available()), sizeof(tmp));
    this->read_array(tmp, toread);  // Discard data
  }
}

/**
 * uart_write_data
 * Write data to UART.type uint8_t *datas: Data to write.
 */
void C4002Component::uart_write_data(uint8_t *datas, size_t len) {
  uart_clear_buffer();
  this->write_array(datas, len);
}

/**
 * uart_read_raw
 * Read raw bytes from UART into buf until timeout or buffer full.
 * Returns number of bytes written (excluding final NUL).
 *
 * Note: bufsize should be >= 2 (we reserve one byte for terminating NUL).
 */
size_t C4002Component::uart_read_raw(uint8_t *buf, size_t bufsize, uint32_t timeout_ms) {
  if (!buf)
    return 0;
  size_t idx = 0;
  uint32_t start = millis();
  buf[0] = '\0';
  while ((millis() - start) < timeout_ms && idx < bufsize) {
    size_t avail = this->available();
    if (avail > 0) {
      size_t toread = std::min(avail, bufsize - idx);
      this->read_array(buf + idx, toread);
      idx += toread;
      if (idx >= bufsize)
        break;
      // Continue reading until timeout or buffer full
      continue;
    }
    // No data available, short delay
    delay(1);
  }
  buf[idx] = '\0';
  return idx;
}

/**
 * set_resolution_mode
 * Set the resolution mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::get_detect_range() {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 4;
  send_date[data_len++] = CMD_SET_DETECT_RANGE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;

  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack rec_pack = recv_pack();

  if (SUCCEED == rec_pack.resPonCode) {
    this->current_detection_range_min_ = (float) ((rec_pack.data[1] << 8) | rec_pack.data[0]) * 0.01;
    this->current_detection_range_max_ = (float) ((rec_pack.data[3] << 8) | rec_pack.data[2]) * 0.01;
    ESP_LOGD(TAG, "get detect range min %f max %f", this->current_detection_range_min_,
             this->current_detection_range_max_);

    return true;
  } else {
    return false;
  }
}

/**
 * set_area_range
 * Set the area range of the device.
 */
void C4002Component::set_area_range(RangValue range_value, float range) { current_area_[range_value] = range; }

/**
 * get_area_range
 * Get the area range of the device.
 */
float C4002Component::get_area_range(RangValue range_value) { return current_area_[range_value]; }

/**
 * joint_enable_door
 * Enable the door according to the current area range.
 */
bool C4002Component::joint_enable_door() {
  // int door_count = 15;

  for (auto &v : enable_door_) {
    v = 1;
  }

  auto apply_range = [this](int min_index, int max_index) {
    float min = current_area_[min_index];
    float max = current_area_[max_index];

    if (min == -1 || max == -1)
      return;
    if (min > max)
      std::swap(min, max);

    if (min < 0)
      min = 0;
    if (max < 0)
      max = 0;

    for (int door = 0; door < 15; door++) {
      if (interval_point_[door] > min && max > interval_point_[door]) {
        enable_door_[door] = 0;
      }
    }
  };

  apply_range(AREA1_DOOR_MIN, AREA1_DOOR_MAX);
  apply_range(AREA2_DOOR_MIN, AREA2_DOOR_MAX);
  apply_range(AREA3_DOOR_MIN, AREA3_DOOR_MAX);

  // for (int i = 0; i < door_count; ++i) {
  //   ESP_LOGD(TAG, "door %d enable %d", i, enable_door_[i]);
  // }

  return enable_all_distance_door(enable_door_);
}

void C4002Component::publish_text(const std::string &msg) {
#ifdef USE_TEXT_SENSOR
  if (this->text_sensor_ != nullptr) {
    this->text_sensor_->publish_state(msg);
  }
#else
  (void) msg;
#endif
}

/**
 * setup_number
 * Set the detect range of the device.
 */
void C4002Component::setup_number() { get_detect_range(); }

/**
 * get_light_threshold
 * Get the light threshold of the device.
 */
float C4002Component::get_light_threshold() {
  float threshold = 0.0;
  uint8_t send_date[10];

  uint16_t data_len = 4;
  send_date[0] = CMD_SET_LIGHT_THRESHOLD;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;

  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);
  RecvPack rec_pack = recv_pack();
  if (SUCCEED == rec_pack.resPonCode) {
    threshold = (float) ((rec_pack.data[1] << 8) | rec_pack.data[0]) * 0.1;
  } else {
    ESP_LOGD(TAG, "get light threshold failed");
  }
  return threshold;
}

#ifdef USE_NUMBER
/* config min detect range */
bool C4002Component::set_min_range(float range) {
  uint16_t closest = (uint16_t) (range * 100);
  uint16_t farthest = (uint16_t) (this->max_detect_range_ * 100);
  if (!set_detect_range(closest, farthest)) {
    return false;
  } else {
    this->min_detect_range_ = range;
    return true;
  }
}
/* config max detect range */
bool C4002Component::set_max_range(float range) {
  uint16_t closest = (uint16_t) (this->min_detect_range_ * 100);
  uint16_t farthest = (uint16_t) (range * 100);
  if (!set_detect_range(closest, farthest)) {
    return false;
  } else {
    this->max_detect_range_ = range;
    return true;
  }
}
#endif

}  // namespace dfrobot_c4002
}  // namespace esphome
