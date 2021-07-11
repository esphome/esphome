#pragma once
#include "esphome/components/remote_base/midea_protocol.h"

namespace esphome {
namespace midea_ac {

using IrData = remote_base::MideaData;

class IrFollowMeData : public IrData {
 public:
  // Default constructor (temp: 30C, beeper: off)
  IrFollowMeData() : IrData({MIDEA_TYPE_FOLLOW_ME, 0x82, 0x48, 0x7F, 0x1F}) {}
  // Copy from Base
  IrFollowMeData(const IrData &data) : IrData(data) {}
  // Direct from temperature and beeper values
  IrFollowMeData(uint8_t temp, bool beeper = false) : IrFollowMeData() {
    this->set_temp(temp);
    this->set_beeper(beeper);
  }

  /* TEMPERATURE */
  uint8_t temp() const { return this->data_[4] - 1; }
  void set_temp(uint8_t val) { this->data_[4] = std::min(MAX_TEMP, val) + 1; }

  /* BEEPER */
  bool beeper() const { return this->data_[3] & 128; }
  void set_beeper(bool val) { this->set_value_(3, 1, 7, val); }

 protected:
  static const uint8_t MAX_TEMP = 37;
};

class IrSpecialData : public IrData {
 public:
  IrSpecialData(uint8_t code) : IrData({MIDEA_TYPE_SPECIAL, code, 0xFF, 0xFF, 0xFF}) {}
};

#if 0
class IrCommandData : public IrData {
 public:
  // Default constructor: power: on, mode: auto, fan: auto, temp: 25C, all timers: off
  IrCommandData() : IrData({MIDEA_TYPE_COMMAND, 0x82, 0x48, 0xFF, 0xFF}) {}
  // Copy from Base
  IrCommandData(const IrData &data) : IrData(data) {}
  // Midea modes enum
  enum MideaMode : uint8_t {
    MIDEA_MODE_COOL,
    MIDEA_MODE_DRY,
    MIDEA_MODE_AUTO,
    MIDEA_MODE_HEAT,
    MIDEA_MODE_FAN,
  };
  // Midea fan enum
  enum MideaFan : uint8_t {
    MIDEA_FAN_AUTO,
    MIDEA_FAN_LOW,
    MIDEA_FAN_MEDIUM,
    MIDEA_FAN_HIGH,
  };
  // Set temperature setpoint
  void set_temp(uint8_t val) { this->data_[2] = std::min(MAX_TEMP, std::max(MIN_TEMP, val)) - MIN_TEMP; }
  // Set mode
  void set_mode(MideaMode mode) { set_value_(1, 0b111, 0, mode); }
  // Set fan speed
  void set_fan_speed(MideaFan fan) { set_value_(1, 0b11, 3, fan); }
  // Set sleep
  void set_sleep(bool val) { set_value_(1, 0b1, 6, val); }
  // Set power
  void set_power(bool val) { set_value_(1, 0b1, 7, val); }
  // Set ON timer (0: disable)
  void set_on_timer(uint16_t minutes) {
    uint8_t halfhours = std::min<uint16_t>(24 * 60, minutes) / 30;
    this->data_[4] = halfhours ? ((halfhours - 1) * 2 + 1) : 0xFF;
  }
  // Set OFF timer (0: disable)
  void set_off_timer(uint16_t minutes) {
    uint8_t halfhours = std::min<uint16_t>(24 * 60, minutes) / 30;
    this->set_value_(3, 0b111111, 1, halfhours ? (halfhours - 1) : 0b111111);
  }

 protected:
  static const uint8_t MIN_TEMP = 17;
  static const uint8_t MAX_TEMP = 30;
};
#endif

}  // namespace midea_ac
}  // namespace esphome
