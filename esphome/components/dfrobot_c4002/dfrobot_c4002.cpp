#include "dfrobot_c4002.h"
#include <string>

namespace esphome {
namespace dfrobot_c4002 {

static const char *const TAG = "dfrobot_c4002";

/**
 * setup
 * Called once when the component is initialized.
 * We call update_config_param() to load device configuration and publish initial values.
 */
void C4002Component::setup() { update_config_param(); }

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

  RetResult ret = get_note_info_loop();
  if (ret.noteType == NOTE_INFO_RESULT) {
    ESP_LOGD(TAG, "******run print NOTE_INFO_RESULT*********");
  } else if (ret.noteType == NOTE_INFO_CALIBRATION) {
    ESP_LOGD(TAG, "********Calibration countdown: %2d s**********", ret.calibCountdown);
    if (ret.calibCountdown == 0) {
      ESP_LOGD(TAG, "Calibration complete!");
    }
  }

  if (now - last_time >= 1000) {  // Execute every 1000ms
    last_time = now;
    get_data();
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
void C4002Component::get_data(void) {
  /** 1.获取数据帧 **/
  /** 2.解析数据 **/
  /** 3.保存数据在类内变量中 **/
  ExistTgt exit_taget_data = get_exist_target_info();
  MoveTgt move_taget_data = get_move_target_info();
  TargetState target_state = get_target_state();

  /** 4.判断数据是否有效，如果有效，发布数据到home assistant上通过回调 **/
  for (auto &listener : this->listeners_) {
    if (listener != nullptr) {
      listener->on_movement_distance(move_taget_data.distance);
      listener->on_movement_speed(move_taget_data.speed);
      listener->on_movement_direction(static_cast<float>(move_taget_data.direction));
      listener->on_existing_distance(exit_taget_data.distance);
      if (target_state == NO_BODY) {
        listener->on_target_state(false);
      } else {
        listener->on_target_state(true);
      }
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

  while (!begin()) {
    delay(1000);
    ESP_LOGD(TAG, "C4002 begin failed");
  }
  ESP_LOGD(TAG, "C4002 begin success");

  setup_number();

  /* 其他参数初始化 */
  float current_light_threshold = get_light_threshold();

  if (min_range_number_ != nullptr) {
    min_range_number_->publish_state(current_detection_range_min_);
    ESP_LOGD(TAG, "Publishing min_range_: %.2f", min_detect_range_);
  }
  if (max_range_number_ != nullptr) {
    max_range_number_->publish_state(current_detection_range_max_);
    ESP_LOGD(TAG, "Publishing max_range_: %.2f", max_detect_range_);
  }
  if (light_threshold_number_ != nullptr) {
    light_threshold_number_->publish_state(current_light_threshold);
    ESP_LOGD(TAG, "Publishing light_threshold_: %.2f", light_threshold_number_);
  }
  if (area1_min_range_number_ != nullptr) {
    area1_min_range_number_->publish_state(current_area_[AREA1_DOOR_MIN] + 1);
  }
  if (area2_min_range_number_ != nullptr) {
    area2_min_range_number_->publish_state(current_area_[AREA2_DOOR_MIN] + 1);
  }
  if (area3_min_range_number_ != nullptr) {
    area3_min_range_number_->publish_state(current_area_[AREA3_DOOR_MIN] + 1);
  }
  if (area1_max_range_number_ != nullptr) {
    area1_max_range_number_->publish_state(current_area_[AREA1_DOOR_MAX] + 1);
  }
  if (area2_max_range_number_ != nullptr) {
    area2_max_range_number_->publish_state(current_area_[AREA2_DOOR_MAX] + 1);
  }
  if (area3_max_range_number_ != nullptr) {
    area3_max_range_number_->publish_state(current_area_[AREA3_DOOR_MAX] + 1);
  }
  if (run_led_switch_ != nullptr) {
    set_run_led(LED_ON);
    run_led_switch_->publish_state((bool) LED_ON);
  }
  if (out_led_switch_ != nullptr) {
    set_out_led(LED_OFF);
    out_led_switch_->publish_state((bool) LED_OFF);
  }

  if (set_report_period(10)) {
    ESP_LOGD(TAG, "set report period success");
  } else {
    ESP_LOGD(TAG, "set report period failed");
  }

  return;
}

bool C4002Component::get_out_mode(void) {
  uint8_t send_date[10];
  uint16_t data_len = 4;
  send_date[0] = CMD_CONFIG_OUT_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    out_mode_ = (OutMode) recPack.data[0];
    return true;
  } else {
    return false;
  }
}

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

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    out_mode_ = out_mode;
    return true;
  } else {
    return false;
  }
}

bool C4002Component::set_light_threshold(float threshold) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 6;
  send_date[data_len++] = CMD_SET_LIGHT_THRESHOLD;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  uint16_t thresholdTemp = (uint16_t) (threshold * 10);
  send_date[data_len++] = thresholdTemp >> 0 & 0xFF;
  send_date[data_len++] = thresholdTemp >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

bool C4002Component::factory_reset(void) {
  uint8_t send_date[10];
  uint16_t data_len = 5;

  send_date[0] = CMD_FACTORY_RESET;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_date[4] = 0x00;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

bool C4002Component::set_resolution_mode(ResolutionMode mode) {
  uint8_t send_date[10];
  uint16_t data_len = 5;
  send_date[0] = CMD_GET_AND_SET_RESOLUTION_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_date[4] = (uint8_t) mode;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    resolution_mode_ = mode;
    return true;
  } else {
    return false;
  }
}

bool C4002Component::enable_distance_door(DistanceDoorType door_type, uint8_t *door_data) {
  uint8_t send_date[40];
  uint16_t data_len = 0;
  uint16_t temp = 5;
  int doorNum = 0;
  if (resolution_mode_ == RESOLUTION_80CM) {
    doorNum = 15;
  } else if (resolution_mode_ == RESOLUTION_20CM) {
    doorNum = 25;
  }
  temp += doorNum;

  send_date[data_len++] = CMD_SET_DISTANCE_DOOR;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = (uint8_t) door_type;
  for (int i = 0; i < doorNum; i++) {
    send_date[data_len++] = door_data[i];
  }
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

bool C4002Component::set_detect_range(uint16_t closest, uint16_t farthest)  // 0-1200cm
{
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 8;
  uint16_t closestTemp = closest, farthestTemp = farthest;
  send_date[data_len++] = CMD_SET_DETECT_RANGE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;

  if (farthestTemp > 1200) {
    farthestTemp = 1200;
  }
  if (closestTemp > farthestTemp) {
    return false;
  }
  send_date[data_len++] = closestTemp >> 0 & 0xFF;
  send_date[data_len++] = closestTemp >> 8 & 0xFF;
  send_date[data_len++] = farthestTemp >> 0 & 0xFF;
  send_date[data_len++] = farthestTemp >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_WRITE_REQUSET);

  RecvPack recPack = recv_pack();

  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

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

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

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

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
}

TargetState C4002Component::get_target_state(void) { return (TargetState) _detectResult.targetStatus; }

float C4002Component::get_light(void) { return ((float) _detectResult.light * 0.1); }

uint32_t C4002Component::get_exist_dist_index(void) { return _detectResult.existDistIndex; }

ExistTgt C4002Component::get_exist_target_info(void) {
  ExistTgt info;
  info.distance = ((float) _detectResult.existTargetDist * 0.01);
  info.energy = _detectResult.existTargetEnery;
  return info;
}

MoveTgt C4002Component::get_move_target_info(void) {
  MoveTgt info;
  info.distance = ((float) _detectResult.moveTargetDist * 0.01);
  info.energy = _detectResult.moveTargetEnery;
  info.speed = ((float) _detectResult.moveTargetSpeed * 0.01);
  info.direction = (MoveDirection) _detectResult.moveTargetDirect;
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
  if (ret == false) {
    return false;
  }
  delay(10);
  ret = set_resolution_mode(resolution_mode_);
  if (ret == false) {
    return false;
  }
  ret = enable_all_distance_door(enable_door_);
  return ret;
}

bool C4002Component::enable_all_distance_door(uint8_t *door_data) {
  bool ret = false;
  ret = enable_distance_door(MOVE_DIST_DOOR, door_data);
  if (ret == false) {
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
RetResult C4002Component::get_note_info_loop(void) {
  RetResult ret;
  RecvPack recData = recv_pack();

  if (SUCCEED == recData.resPonCode) {
    if (recData.packType == FRAME_TYPE_NOTIFICATION) {  // note
      if (recData.dataHeader.cmd == NOTE_RESULT_CMD) {
        // memcpy(&this->_detectResult, recData.data, sizeof(DetectRet));
        this->_detectResult.targetStatus = recData.data[0];
        this->_detectResult.light = recData.data[2] << 8 | recData.data[1];
        this->_detectResult.existDistIndex =
            recData.data[6] << 24 | recData.data[5] << 16 | recData.data[4] << 8 | recData.data[3];
        this->_detectResult.existCountDown = recData.data[8] << 8 | recData.data[7];
        this->_detectResult.existTargetDist = recData.data[10] << 8 | recData.data[9];
        this->_detectResult.existTargetEnery = recData.data[11];
        this->_detectResult.moveTargetDist = recData.data[13] << 8 | recData.data[12];
        this->_detectResult.moveTargetSpeed = recData.data[15] << 8 | recData.data[14];
        this->_detectResult.moveTargetEnery = recData.data[16];
        this->_detectResult.moveTargetDirect = recData.data[17];
        ret.noteType = NOTE_INFO_RESULT;
      } else if (recData.dataHeader.cmd == NOTE_ENVIRNMENT_CALIBRATION_CMD) {
        ret.calibCountdown = recData.data[1] << 8 | recData.data[0];
        ret.noteType = NOTE_INFO_CALIBRATION;
      } else {
        ret.noteType = NO_NOTE;
      }
    }
  }
  return ret;
}

/**
 * get_resolution_mode
 * Get the resolution mode of the device.
 * Returns true if successful, false otherwise.
 */
bool C4002Component::get_resolution_mode(void) {
  uint8_t send_date[10];
  uint16_t data_len = 4;
  send_date[0] = CMD_GET_AND_SET_RESOLUTION_MODE;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;
  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    resolution_mode_ = (ResolutionMode) recPack.data[0];
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

  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    return true;
  } else {
    return false;
  }
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
  uint16_t checkSum = 0;

  send_date[data_len++] = FRAME_HEADER1;
  send_date[data_len++] = FRAME_HEADER2;
  send_date[data_len++] = FRAME_HEADER3;
  send_date[data_len++] = FRAME_HEADER4;
  uint16_t temp = len + 10;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;
  send_date[data_len++] = 0x00;
  send_date[data_len++] = msg_type;
  memcpy(&send_date[data_len], pdata, len);
  data_len += len;
  checkSum = getCheckSum((uint8_t *) send_date, data_len);

  send_date[data_len++] = checkSum >> 0 & 0xFF;
  send_date[data_len++] = checkSum >> 8 & 0xFF;

  uart_write_data(send_date, (size_t) data_len);
}

/**
 * recvPack
 * Read a data frame from the UART and parse it.
 * Returns a RecvPack struct with the parsed data.
 */
RecvPack C4002Component::recv_pack() {
  RecvPack recvDat;
  memset(&recvDat, 0, sizeof(recvDat));
  uint8_t *pdata = (uint8_t *) malloc(60 * sizeof(uint8_t));
  if (pdata == NULL) {
    recvDat.packType = FRAME_ERROR;
    return recvDat;
  }

  size_t recvLen = uart_read_raw(pdata, 8, 20);

  if (recvLen == 8 && pdata[0] == FRAME_HEADER1 && pdata[1] == FRAME_HEADER2 && pdata[2] == FRAME_HEADER3 &&
      pdata[3] == FRAME_HEADER4) {
    size_t packLen = (pdata[5] << 8) | pdata[4];

    recvLen = uart_read_raw(&pdata[8], (size_t) (packLen - 8), 20);
    // ESP_LOGD(TAG, "recvLen: %d", recvLen);

    if (recvLen == (packLen - 8)) {
      recvDat.packType = pdata[7];
      if (checkSum(pdata, packLen)) {
        uint16_t data_len = (pdata[11] << 8) | pdata[10];

        memcpy(&recvDat, &pdata[8], data_len);
        recvDat.resPonCode = (ResponseCode) recvDat.dataHeader.respCode;

        if (recvDat.packType == FRAME_TYPE_NOTIFICATION) {  // note
          ESP_LOGD(TAG, "get note result");
        } else if (recvDat.packType == FRAME_TYPE_WRITE_RESPOND) {  // write
          ESP_LOGD(TAG, "get write respond");
        } else if (recvDat.packType == FRAME_TYPE_READ_RESPOND) {  // read
          ESP_LOGD(TAG, "get read respond");
        } else {
          ESP_LOGD(TAG, "this is error pack");
          recvDat.resPonCode = CMD_ERR;
        }

      } else {
        recvDat.resPonCode = AUTHENTICATION_ERR;
        ESP_LOGD(TAG, "Authentication error");
      }
    } else {
      recvDat.resPonCode = DATALEN_ERR;
      ESP_LOGD(TAG, " recvLen error");
    }
  } else {
    recvDat.resPonCode = AUTHENTICATION_ERR;
    // ESP_LOGD(TAG, "Authentication error");
  }
  free(pdata);
  return recvDat;
}

/**
 * checkSum
 * Check the checksum of the data.
 */
bool C4002Component::checkSum(uint8_t *pdata, uint8_t len) {
  uint16_t calculateParity = 0;

  for (uint8_t i = 0; i < len - 2; i++) {
    calculateParity += pdata[i];
  }
  uint16_t temp = (pdata[len - 1] << 8) | pdata[len - 2];
  if (calculateParity == temp) {
    return true;
  }
  return false;
}

/**
 * getCheckSum
 * Calculate the checksum of the data.
 */
uint16_t C4002Component::getCheckSum(uint8_t *pdata, uint16_t len) {
  uint16_t Parity = 0;
  for (uint16_t i = 0; i < len; i++) {
    Parity += pdata[i];
  }
  return Parity;
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

bool C4002Component::get_detect_range(void) {
  uint8_t send_date[10];
  uint16_t data_len = 0;
  uint16_t temp = 4;
  send_date[data_len++] = CMD_SET_DETECT_RANGE;
  send_date[data_len++] = READ_AND_WRITE_REQ;
  send_date[data_len++] = temp >> 0 & 0xFF;
  send_date[data_len++] = temp >> 8 & 0xFF;

  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);

  RecvPack recPack = recv_pack();

  if (SUCCEED == recPack.resPonCode) {
    this->current_detection_range_min_ = (float) ((recPack.data[1] << 8) | recPack.data[0]) * 0.01;
    this->current_detection_range_max_ = (float) ((recPack.data[3] << 8) | recPack.data[2]) * 0.01;
    return true;
  } else {
    return false;
  }
}

void C4002Component::set_area_range(RangValue range_value, float range) {
  current_area_[range_value] = range - 1;
  return;
}

float C4002Component::get_area_range(RangValue range_value) { return current_area_[range_value] + 1; }

bool C4002Component::joint_enable_door(void) {
  for (int i = 0; i < 11; ++i) {
    enable_door_[i] = 0;
  }

  auto apply_range = [this](int min_index, int max_index) {
    int start = static_cast<int>(this->current_area_[min_index]);
    int end = static_cast<int>(this->current_area_[max_index]);

    if (start > end) {
      std::swap(start, end);
    }

    if (start < 0)
      start = 0;
    if (end < 0)
      end = 0;
    if (start > 10)
      start = 10;
    if (end > 10)
      end = 10;

    // 关键在这里：<=，表示闭区间 [start, end]
    for (int door = start; door <= end && door < 11; ++door) {
      this->enable_door_[door] = 1;
    }
  };

  apply_range(AREA1_DOOR_MIN, AREA1_DOOR_MAX);  // 第 1 个区域
  apply_range(AREA2_DOOR_MIN, AREA2_DOOR_MAX);  // 第 2 个区域
  apply_range(AREA3_DOOR_MIN, AREA3_DOOR_MAX);  // 第 3 个区域

  return enable_all_distance_door(enable_door_);
}

void C4002Component::setup_number(void) {
  bool ret;
  ret = get_detect_range();
  if (ret == true) {
    ESP_LOGD(TAG, "get detect range success");
  } else {
    ESP_LOGD(TAG, "get detect range failed");
  }
}

float C4002Component::get_light_threshold(void) {
  float threshold = 0.0;
  uint8_t send_date[10];

  uint16_t data_len = 4;
  send_date[0] = CMD_SET_LIGHT_THRESHOLD;
  send_date[1] = READ_AND_WRITE_REQ;
  send_date[2] = data_len >> 0 & 0xFF;
  send_date[3] = data_len >> 8 & 0xFF;

  send_pack(send_date, data_len, FRAME_TYPE_READ_REQUSET);
  RecvPack recPack = recv_pack();
  if (SUCCEED == recPack.resPonCode) {
    threshold = (float) ((recPack.data[1] << 8) | recPack.data[0]) * 0.1;
  } else {
    ESP_LOGD(TAG, "get light threshold failed");
  }
  return threshold;
}

#ifdef USE_NUMBER

bool C4002Component::set_min_range(float range) {
  uint16_t closest = (uint16_t) (range * 100);
  uint16_t farthest = (uint16_t) (this->max_detect_range_ * 100);
  if (set_detect_range(closest, farthest) == false) {
    return false;
  } else {
    this->min_detect_range_ = range;
    return true;
  }
}

bool C4002Component::set_max_range(float range) {
  uint16_t closest = (uint16_t) (this->min_detect_range_ * 100);
  uint16_t farthest = (uint16_t) (range * 100);
  if (set_detect_range(closest, farthest) == false) {
    return false;
  } else {
    this->max_detect_range_ = range;
    return true;
  }
}
#endif

}  // namespace dfrobot_c4002
}  // namespace esphome
