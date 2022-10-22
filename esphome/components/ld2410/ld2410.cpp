#include "ld2410.h"

#define highbyte(val) (uint8_t)((val) >> 8)
#define lowbyte(val) (uint8_t)((val) &0xff)

namespace esphome {
namespace ld2410 {

static const char *const TAG = "ld2410";

void LD2410Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2410:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "HasTargetSensor", target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MovingSensor", moving_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillSensor", still_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Moving Distance", moving_target_distance_sensor_);
  LOG_SENSOR("  ", "Still Distance", still_target_distance_sensor_);
  LOG_SENSOR("  ", "Moving Energy", moving_target_energy_sensor_);
  LOG_SENSOR("  ", "Still Energy", still_target_energy_sensor_);
  LOG_SENSOR("  ", "Detection Distance", detection_distance_sensor_);
#endif
}

void LD2410Component::setup() {
  set_update_interval(15000);
  this->set_config_mode_(true);
  this->set_max_distances_none_duration_(this->maxMoveDistance_, this->maxStillDistance_, this->noneDuration_);
  // Configure Gates sensitivity
  this->set_sensitivity_(0, this->rg0_move_sens_, this->rg0_still_sens_);
  this->set_sensitivity_(1, this->rg1_move_sens_, this->rg1_still_sens_);
  this->set_sensitivity_(2, this->rg2_move_sens_, this->rg2_still_sens_);
  this->set_sensitivity_(3, this->rg3_move_sens_, this->rg3_still_sens_);
  this->set_sensitivity_(4, this->rg4_move_sens_, this->rg4_still_sens_);
  this->set_sensitivity_(5, this->rg5_move_sens_, this->rg5_still_sens_);
  this->set_sensitivity_(6, this->rg6_move_sens_, this->rg6_still_sens_);
  this->set_sensitivity_(7, this->rg7_move_sens_, this->rg7_still_sens_);
  this->set_sensitivity_(8, this->rg8_move_sens_, this->rg8_still_sens_);
  this->set_config_mode_(false);
}

void LD2410Component::loop() {
  const int max_line_length = 80;
  static char buffer[max_line_length];

  while (available()) {
    this->readline_(read(), buffer, max_line_length);
  }
}

// void LD2410Component::setNumbers(Number *maxMovingDistanceRange_, Number *maxStillDistanceRange_, Number
// *noneDuration_) {
//   maxMovingDistanceRange = maxMovingDistanceRange_;
//   maxStillDistanceRange = maxStillDistanceRange_;
//   noneDuration = noneDuration_;
// }

void LD2410Component::sendCommand_(uint8_t command, char *command_value, int command_value_len) {
  // lastCommandSuccess->publish_state(false);

  // frame start bytes
  this->write_array(CMD_FRAME_HEADER, 4);
  // length bytes
  int len = 2;
  if (command_value != nullptr)
    len += command_value_len;
  this->write_byte(lowbyte(len));
  this->write_byte(highbyte(len));

  // command
  this->write_byte(lowbyte(command));
  this->write_byte(highbyte(command));

  // command value bytes
  if (command_value != nullptr) {
    for (int i = 0; i < command_value_len; i++) {
      this->write_byte(command_value[i]);
    }
  }
  // frame end bytes
  this->write_array(CMD_FRAME_END, 4);
  delay(50);
}

void LD2410Component::handlePeriodicData_(char *buffer, int len) {
  if (len < 12)
    return;  // 4 frame start bytes + 2 length bytes + 1 data end byte + 1 crc byte + 4 frame end bytes
  if (buffer[0] != 0xF4 || buffer[1] != 0xF3 || buffer[2] != 0xF2 || buffer[3] != 0xF1)
    return;  // check 4 frame start bytes
  if (buffer[7] != 0xAA || buffer[len - 6] != 0x55 || buffer[len - 5] != 0x00)
    return;  // data head=0xAA, data end=0x55, crc=0x00
  /*
    Data Type: 6th byte
    0x01: Engineering mode
    0x02: Normal mode
  */
  char data_type = buffer[DATA_TYPES];
  /*
    Target states: 9th byte
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  char target_state = buffer[TARGET_STATES];
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(target_state != 0x00);
  }

  /*
    Reduce data update rate to prevent home assistant database size glow fast
  */
  int32_t current_millis = millis();
  if (current_millis - last_periodic_millis < 1000)
    return;
  last_periodic_millis = current_millis;

  if (this->moving_binary_sensor_ != nullptr) {
    this->moving_binary_sensor_->publish_state(CHECK_BIT(target_state, 0));
  }
  if (this->still_binary_sensor_ != nullptr) {
    this->still_binary_sensor_->publish_state(CHECK_BIT(target_state, 1));
  }
  /*
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
  */
  if (this->moving_target_distance_sensor_ != nullptr) {
    int new_moving_target_distance = this->twoByteToInt_(buffer[MOVING_TARGET_LOW], buffer[MOVING_TARGET_HIGH]);
    if (this->moving_target_distance_sensor_->get_state() != new_moving_target_distance)
      this->moving_target_distance_sensor_->publish_state(new_moving_target_distance);
  }
  if (this->moving_target_energy_sensor_ != nullptr) {
    int new_moving_target_energy = buffer[MOVING_ENERGY];
    if (this->moving_target_energy_sensor_->get_state() != new_moving_target_energy)
      this->moving_target_energy_sensor_->publish_state(new_moving_target_energy);
  }
  if (this->still_target_distance_sensor_ != nullptr) {
    int new_still_target_distance = this->twoByteToInt_(buffer[STILL_TARGET_LOW], buffer[STILL_TARGET_HIGH]);
    if (this->still_target_distance_sensor_->get_state() != new_still_target_distance)
      this->still_target_distance_sensor_->publish_state(new_still_target_distance);
  }
  if (this->still_target_energy_sensor_ != nullptr) {
    int new_still_target_energy = buffer[STILL_ENERGY];
    if (this->still_target_energy_sensor_->get_state() != new_still_target_energy)
      this->still_target_energy_sensor_->publish_state(new_still_target_energy);
  }
  if (this->detection_distance_sensor_ != nullptr) {
    int new_detect_distance = this->twoByteToInt_(buffer[DETECT_DISTANCE_LOW], buffer[DETECT_DISTANCE_HIGH]);
    if (this->detection_distance_sensor_->get_state() != new_detect_distance)
      this->detection_distance_sensor_->publish_state(new_detect_distance);
  }
  // if (dataType == 0x01) {  // engineering mode
  //                          // todo: support engineering mode data
  // }
}

void LD2410Component::handleACKData_(char *buffer, int len) {
  ESP_LOGI(TAG, "Handling ACK DATA for COMMAND");
  if (len < 10)
    return;
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA)
    return;  // check 4 frame start bytes
  if (buffer[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Error with last command");
    return;
  }
  if (this->twoByteToInt_(buffer[8], buffer[9]) != 0x00) {
    ESP_LOGE(TAG, "Error with last command");
    return;
  }

  switch (buffer[COMMAND]) {
    case lowbyte(CMD_ENABLE_CONF):
      ESP_LOGD(TAG, "Handled Enable conf command");
      break;
    case lowbyte(CMD_DISABLE_CONF):
      ESP_LOGD(TAG, "Handled Disabled conf command");
      break;
    case lowbyte(CMD_VERSION):
      // TODO ESP_LOGI(TAG, "Version is V: %u.%u", buffer[13], buffer[12]);
      break;
    case lowbyte(CMD_GATE_SENS):
      ESP_LOGD(TAG, "Handled sensitivity command");
      break;
    case lowbyte(CMD_QUERY):  // Query parameters response
    {
      if (buffer[10] != 0xAA)
        return;  // value head=0xAA
      /*
        Moving distance range: 13th byte
        Still distance range: 14th byte
      */
      // TODO
      // maxMovingDistanceRange->publish_state(buffer[12]);
      // maxStillDistanceRange->publish_state(buffer[13]);
      /*
        Moving Sensitivities: 15~23th bytes
        Still Sensitivities: 24~32th bytes
      */
      for (int i = 0; i < 9; i++) {
        moving_sensitivities[i] = buffer[14 + i];
      }
      for (int i = 0; i < 9; i++) {
        still_sensitivities[i] = buffer[23 + i];
      }
      /*
        None Duration: 33~34th bytes
      */
      // noneDuration->publish_state(this->twoByteToInt_(buffer[32], buffer[33]));
    } break;
    default:
      break;
  }
}

void LD2410Component::readline_(int readch, char *buffer, int len) {
  static int pos = 0;

  if (readch >= 0) {
    if (pos < len - 1) {
      buffer[pos++] = readch;
      buffer[pos] = 0;
    } else {
      pos = 0;
    }
    if (pos >= 4) {
      if (buffer[pos - 4] == 0xF8 && buffer[pos - 3] == 0xF7 && buffer[pos - 2] == 0xF6 && buffer[pos - 1] == 0xF5) {
        ESP_LOGV(TAG, "Will handle Periodic Data");
        this->handlePeriodicData_(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      } else if (buffer[pos - 4] == 0x04 && buffer[pos - 3] == 0x03 && buffer[pos - 2] == 0x02 &&
                 buffer[pos - 1] == 0x01) {
        ESP_LOGV(TAG, "Will handle ACK Data");
        this->handleACKData_(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      }
    }
  }
}

void LD2410Component::set_config_mode_(bool enable) {
  uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  char value[2] = {0x01, 0x00};
  this->sendCommand_(cmd, enable ? value : nullptr, 2);
}

void LD2410Component::queryParameters_() { this->sendCommand_(CMD_QUERY, nullptr, 0); }
void LD2410Component::getVersion_() { this->sendCommand_(CMD_VERSION, nullptr, 0); }

void LD2410Component::update() {}

// void LD2410Component::setEngineeringMode(bool enable) {
//   char cmd[2] = {enable ? 0x62  : 0x63, 0x00};
//   sendCommand_(cmd, nullptr, 0);
// }

void LD2410Component::set_max_distances_none_duration_(int max_moving_distance_range, int max_still_distance_range,
                                                       int none_duration) {
  // char cmd[2] = {0x60, 0x00};
  // uint8_t cmd = CMD_MAXDIST_DURATION;
  char value[18] = {0x00,
                    0x00,
                    lowbyte(max_moving_distance_range),
                    highbyte(max_moving_distance_range),
                    0x00,
                    0x00,
                    0x01,
                    0x00,
                    lowbyte(max_still_distance_range),
                    highbyte(max_still_distance_range),
                    0x00,
                    0x00,
                    0x02,
                    0x00,
                    lowbyte(none_duration),
                    highbyte(none_duration),
                    0x00,
                    0x00};
  this->sendCommand_(CMD_MAXDIST_DURATION, value, 18);
  this->queryParameters_();
}
void LD2410Component::set_sensitivity_(uint8_t gate, uint8_t motionsens, uint8_t stillsens) {
  // reference
  // https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-?spm=a2g0o.detail.1000023.17.93465697yFwVxH
  //   Send data: configure the motion sensitivity of distance gate 3 to 40, and the static sensitivity of 40
  // 00 00 (gate)
  // 03 00 00 00 (gate number)
  // 01 00 (motion sensitivity)
  // 28 00 00 00 (value)
  // 02 00 (still sensitivtiy)
  // 28 00 00 00 (value)
  char value[18] = {0x00, 0x00, lowbyte(gate),       highbyte(gate),       0x00, 0x00,
                    0x01, 0x00, lowbyte(motionsens), highbyte(motionsens), 0x00, 0x00,
                    0x02, 0x00, lowbyte(stillsens),  highbyte(stillsens),  0x00, 0x00};
  this->sendCommand_(CMD_GATE_SENS, value, 18);
}
// void ld2410::factoryReset() {
//   char cmd[2] = {0xA2, 0x00};
//   sendCommand_(cmd, nullptr, 0);
// }

// void ld2410::reboot() {
//   char cmd[2] = {0xA3, 0x00};
//   sendCommand_(cmd, nullptr, 0);
//   // not need to exit config mode because the ld2410 will reboot automatically
// }

// void ld2410::setBaudrate(int index) {
//   char cmd[2] = {0xA1, 0x00};
//   char value[2] = {index, 0x00};
//   sendCommand_(cmd, value, 2);
// }

}  // namespace ld2410
}  // namespace esphome
