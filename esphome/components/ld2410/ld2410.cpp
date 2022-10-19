#include "ld2410.h"
// #include "esphome/core/log.h"
// #include "esphome/core/hal.h"

// #define highbyte(val) (uint8_t)((val) >> 8)
// #define lowbyte(val) (uint8_t)((val) &0xff)

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
  this->setConfigMode(true);
  setMaxDistancesAndNoneDuration(this->maxMoveDistance_, this->maxStillDistance_, this->noneDuration_);
  // Configure Gates sensitivity
  setSensitivity(0, rg0_move_sens_, rg0_still_sens_);
  setSensitivity(1, rg1_move_sens_, rg1_still_sens_);
  setSensitivity(2, rg2_move_sens_, rg2_still_sens_);
  setSensitivity(3, rg3_move_sens_, rg3_still_sens_);
  setSensitivity(4, rg4_move_sens_, rg4_still_sens_);
  setSensitivity(5, rg5_move_sens_, rg5_still_sens_);
  setSensitivity(6, rg6_move_sens_, rg6_still_sens_);
  setSensitivity(7, rg7_move_sens_, rg7_still_sens_);
  setSensitivity(8, rg8_move_sens_, rg8_still_sens_);
  setConfigMode(false);
}

void LD2410Component::loop() {
  const int max_line_length = 80;
  static char buffer[max_line_length];

  while (available()) {
    readline(read(), buffer, max_line_length);
  }
}

// void LD2410Component::setNumbers(Number *maxMovingDistanceRange_, Number *maxStillDistanceRange_, Number
// *noneDuration_) {
//   maxMovingDistanceRange = maxMovingDistanceRange_;
//   maxStillDistanceRange = maxStillDistanceRange_;
//   noneDuration = noneDuration_;
// }

void LD2410Component::sendCommand(uint8_t command, char *commandValue, int commandValueLen) {
  // lastCommandSuccess->publish_state(false);

  // frame start bytes
  this->write_array(CMD_FRAME_HEADER, 4);
  // length bytes
  int len = 2;
  if (commandValue != nullptr)
    len += commandValueLen;
  write_byte(lowByte(len));
  write_byte(highByte(len));

  // command
  write_byte(lowByte(command));
  write_byte(highByte(command));

  // command value bytes
  if (commandValue != nullptr) {
    for (int i = 0; i < commandValueLen; i++) {
      write_byte(commandValue[i]);
    }
  }
  // frame end bytes
  this->write_array(CMD_FRAME_END, 4);
  delay(50);
}

void LD2410Component::handlePeriodicData(char *buffer, int len) {
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
  char dataType = buffer[dataTypes];
  /*
    Target states: 9th byte
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  char stateByte = buffer[targetStates];
  if (this->hastargetsensor()) {
    this->target_binary_sensor_->publish_state(stateByte != 0x00);
  }

  /*
    Reduce data update rate to prevent home assistant database size glow fast
  */
  long currentMillis = millis();
  if (currentMillis - lastPeriodicMillis < 1000)
    return;
  lastPeriodicMillis = currentMillis;

  if (this->hasmovingtargetsensor()) {
    this->moving_binary_sensor_->publish_state(CHECK_BIT(stateByte, 0));
  }
  if (this->hasstilltargetsensor()) {
    this->still_binary_sensor_->publish_state(CHECK_BIT(stateByte, 1));
  }
  /*
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
  */
  if (moving_target_distance_sensor_ != nullptr) {
    int newMovingTargetDistance = twoByteToInt(buffer[movingTargetLow], buffer[movingTargetHigh]);
    if (moving_target_distance_sensor_->get_state() != newMovingTargetDistance)
      moving_target_distance_sensor_->publish_state(newMovingTargetDistance);
  }
  if (moving_target_energy_sensor_ != nullptr) {
    int newMovingTargetEnergy = buffer[movingEnergy];
    if (moving_target_energy_sensor_->get_state() != newMovingTargetEnergy)
      moving_target_energy_sensor_->publish_state(newMovingTargetEnergy);
  }
  if (still_target_distance_sensor_ != nullptr) {
    int newStillTargetDistance = twoByteToInt(buffer[stillTargetLow], buffer[stillTargetHigh]);
    if (still_target_distance_sensor_->get_state() != newStillTargetDistance)
      still_target_distance_sensor_->publish_state(newStillTargetDistance);
  }
  if (still_target_energy_sensor_ != nullptr) {
    int newStillTargetEnergy = buffer[stillEnergy];
    if (still_target_energy_sensor_->get_state() != newStillTargetEnergy)
      still_target_energy_sensor_->publish_state(newStillTargetEnergy);
  }
  if (detection_distance_sensor_ != nullptr) {
    int newDetectDistance = twoByteToInt(buffer[detectDistanceLow], buffer[detectDistanceHigh]);
    if (detection_distance_sensor_->get_state() != newDetectDistance)
      detection_distance_sensor_->publish_state(newDetectDistance);
  }
  // if (dataType == 0x01) {  // engineering mode
  //                          // todo: support engineering mode data
  // }
}

void LD2410Component::handleACKData(char *buffer, int len) {
  ESP_LOGI(TAG, "Handling ACK DATA for COMMAND");
  if (len < 10)
    return;
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA)
    return;  // check 4 frame start bytes
  if (buffer[command_status] != 0x01) {
    ESP_LOGE(TAG, "Error wiht last command");
    return;
  }
  if (twoByteToInt(buffer[8], buffer[9]) != 0x00) {
    ESP_LOGE(TAG, "Error with last command");
    return;
  }

  switch (buffer[command]) {
    case lowByte(CMD_ENABLE_CONF):
      ESP_LOGD(TAG, "Handled Enable conf command");
      break;
    case lowByte(CMD_DISABLE_CONF):
      ESP_LOGD(TAG, "Handled Disabled conf command");
      break;
    case lowByte(CMD_VERSION):
      // TODO ESP_LOGI(TAG, "Version is V: %u.%u", buffer[13], buffer[12]);
      break;
    case lowByte(CMD_GATE_SENS):
      ESP_LOGD(TAG, "Handled sensitivity command");
      break;
    case lowByte(CMD_QUERY):  // Query parameters response
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
        movingSensitivities[i] = buffer[14 + i];
      }
      for (int i = 0; i < 9; i++) {
        stillSensitivities[i] = buffer[23 + i];
      }
      /*
        None Duration: 33~34th bytes
      */
      // noneDuration->publish_state(twoByteToInt(buffer[32], buffer[33]));
    } break;
    default:
      break;
  }
}

void LD2410Component::readline(int readch, char *buffer, int len) {
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
        handlePeriodicData(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      } else if (buffer[pos - 4] == 0x04 && buffer[pos - 3] == 0x03 && buffer[pos - 2] == 0x02 &&
                 buffer[pos - 1] == 0x01) {
        ESP_LOGV(TAG, "Will handle ACK Data");
        handleACKData(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      }
    }
  }
  return;
}

void LD2410Component::setConfigMode(bool enable) {
  uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  char value[2] = {0x01, 0x00};
  sendCommand(cmd, enable ? value : nullptr, 2);
}

void LD2410Component::queryParameters() { sendCommand(CMD_QUERY, nullptr, 0); }
void LD2410Component::getVersion() { sendCommand(CMD_VERSION, nullptr, 0); }

void LD2410Component::update() {}

// void LD2410Component::setEngineeringMode(bool enable) {
//   char cmd[2] = {enable ? 0x62  : 0x63, 0x00};
//   sendCommand(cmd, nullptr, 0);
// }

void LD2410Component::setMaxDistancesAndNoneDuration(int maxMovingDistanceRange, int maxStillDistanceRange,
                                                     int noneDuration) {
  // char cmd[2] = {0x60, 0x00};
  // uint8_t cmd = CMD_MAXDIST_DURATION;
  char value[18] = {0x00, 0x00, lowByte(maxMovingDistanceRange), highByte(maxMovingDistanceRange), 0x00, 0x00,
                    0x01, 0x00, lowByte(maxStillDistanceRange),  highByte(maxStillDistanceRange),  0x00, 0x00,
                    0x02, 0x00, lowByte(noneDuration),           highByte(noneDuration),           0x00, 0x00};
  sendCommand(CMD_MAXDIST_DURATION, value, 18);
  queryParameters();
}
void LD2410Component::setSensitivity(uint8_t gate, uint8_t motionsens, uint8_t stillsens) {
  // reference
  // https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-?spm=a2g0o.detail.1000023.17.93465697yFwVxH
  //   Send data: configure the motion sensitivity of distance gate 3 to 40, and the static sensitivity of 40
  // 00 00 (gate)
  // 03 00 00 00 (gate number)
  // 01 00 (motion sensitivity)
  // 28 00 00 00 (value)
  // 02 00 (still sensitivtiy)
  // 28 00 00 00 (value)
  char value[18] = {0x00, 0x00, lowByte(gate),       highByte(gate),       0x00, 0x00,
                    0x01, 0x00, lowByte(motionsens), highByte(motionsens), 0x00, 0x00,
                    0x02, 0x00, lowByte(stillsens),  highByte(stillsens),  0x00, 0x00};
  sendCommand(CMD_GATE_SENS, value, 18);
}
// void ld2410::factoryReset() {
//   char cmd[2] = {0xA2, 0x00};
//   sendCommand(cmd, nullptr, 0);
// }

// void ld2410::reboot() {
//   char cmd[2] = {0xA3, 0x00};
//   sendCommand(cmd, nullptr, 0);
//   // not need to exit config mode because the ld2410 will reboot automatically
// }

// void ld2410::setBaudrate(int index) {
//   char cmd[2] = {0xA1, 0x00};
//   char value[2] = {index, 0x00};
//   sendCommand(cmd, value, 2);
// }

}  // namespace ld2410
}  // namespace esphome
