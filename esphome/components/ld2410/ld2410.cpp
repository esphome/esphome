#include "ld2410.h"
// #include "esphome/core/log.h"
// #include "esphome/core/hal.h"

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
// void ld2410::setNumbers(Number *maxMovingDistanceRange_, Number *maxStillDistanceRange_, Number *noneDuration_) {
//   maxMovingDistanceRange = maxMovingDistanceRange_;
//   maxStillDistanceRange = maxStillDistanceRange_;
//   noneDuration = noneDuration_;
// }

void LD2410Component::sendCommand(char *commandStr, char *commandValue, int commandValueLen) {
  // FIXME
  // lastCommandSuccess->publish_state(false);
  // frame start bytes
  write_byte(0xFD);
  write_byte(0xFC);
  write_byte(0xFB);
  write_byte(0xFA);
  // length bytes
  int len = 2;
  if (commandValue != nullptr)
    len += commandValueLen;
  // FIXME : create high byte & low byte
  // this->write(((uint8_t)((crc16) >> 8)));   // highbyte
  // this->write(((uint8_t)((crc16) &0xff)));  // lowbyte
  // write_byte(lowByte(len));
  // write_byte(highByte(len));
  write_byte((uint8_t)((len) >> 8));
  write_byte((uint8_t)((len) &0xff));
  // command string bytes
  write_byte(commandStr[0]);
  write_byte(commandStr[1]);
  // command value bytes
  if (commandValue != nullptr) {
    for (int i = 0; i < commandValueLen; i++) {
      write_byte(commandValue[i]);
    }
  }
  // frame end bytes
  write_byte(0x04);
  write_byte(0x03);
  write_byte(0x02);
  write_byte(0x01);
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
  char dataType = buffer[5];
  /*
    Target states: 9th byte
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  char stateByte = buffer[8];
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
    int newMovingTargetDistance = twoByteToInt(buffer[9], buffer[10]);
    if (moving_target_distance_sensor_->get_state() != newMovingTargetDistance)
      moving_target_distance_sensor_->publish_state(newMovingTargetDistance);
  }
  if (moving_target_energy_sensor_ != nullptr) {
    int newMovingTargetEnergy = buffer[11];
    if (moving_target_energy_sensor_->get_state() != newMovingTargetEnergy)
      moving_target_energy_sensor_->publish_state(newMovingTargetEnergy);
  }
  if (still_target_distance_sensor_ != nullptr) {
    int newStillTargetDistance = twoByteToInt(buffer[12], buffer[13]);
    if (still_target_distance_sensor_->get_state() != newStillTargetDistance)
      still_target_distance_sensor_->publish_state(newStillTargetDistance);
  }
  if (still_target_energy_sensor_ != nullptr) {
    int newStillTargetEnergy = buffer[14];
    if (still_target_energy_sensor_->get_state() != newStillTargetEnergy)
      still_target_energy_sensor_->publish_state(buffer[14]);
  }
  if (detection_distance_sensor_ != nullptr) {
    int newDetectDistance = twoByteToInt(buffer[15], buffer[16]);
    if (detection_distance_sensor_->get_state() != newDetectDistance)
      detection_distance_sensor_->publish_state(newDetectDistance);
  }
  // if (dataType == 0x01) {  // engineering mode
  //                          // todo: support engineering mode data
  // }
}

void LD2410Component::handleACKData(char *buffer, int len) {
  if (len < 10)
    return;
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA)
    return;  // check 4 frame start bytes
  if (buffer[7] != 0x01)
    return;
  if (twoByteToInt(buffer[8], buffer[9]) != 0x00) {
    // TODO
    // lastCommandSuccess->publish_state(false);
    return;
  }
  // TODO
  // lastCommandSuccess->publish_state(true);
  switch (buffer[6]) {
    case 0x61:  // Query parameters response
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
        handlePeriodicData(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      } else if (buffer[pos - 4] == 0x04 && buffer[pos - 3] == 0x03 && buffer[pos - 2] == 0x02 &&
                 buffer[pos - 1] == 0x01) {
        handleACKData(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      }
    }
  }
  return;
}

// void ld2410::setConfigMode(bool enable) {
//   char cmd[2] = {enable ? 0xFF : 0xFE, 0x00};
//   char value[2] = {0x01, 0x00};
//   sendCommand(cmd, enable ? value : nullptr, 2);
// }

void LD2410Component::queryParameters() {
  char cmd_query[2] = {0x61, 0x00};
  sendCommand(cmd_query, nullptr, 0);
}

void LD2410Component::setup() { set_update_interval(15000); }

void LD2410Component::loop() {
  const int max_line_length = 80;
  static char buffer[max_line_length];
  while (available()) {
    readline(read(), buffer, max_line_length);
  }
}

void LD2410Component::update() {}

// void setEngineeringMode(bool enable) {
//   char cmd[2] = {enable ? 0x62 se : 0x63, 0x00};
//   sendCommand(cmd, nullptr, 0);
// }

// void ld2410::setMaxDistancesAndNoneDuration(int maxMovingDistanceRange, int maxStillDistanceRange, int noneDuration)
// {
//   char cmd[2] = {0x60, 0x00};
//   char value[18] = {0x00, 0x00, lowByte(maxMovingDistanceRange), highByte(maxMovingDistanceRange), 0x00, 0x00,
//                     0x01, 0x00, lowByte(maxStillDistanceRange),  highByte(maxStillDistanceRange),  0x00, 0x00,
//                     0x02, 0x00, lowByte(noneDuration),           highByte(noneDuration),           0x00, 0x00};
//   sendCommand(cmd, value, 18);
//   queryParameters();
// }

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
