#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/remote_base/midea_protocol.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

using remote_base::MideaData;
using remote_base::MideaProtocol;

class MideaFollowMe : public MideaData {
 public:
  // Default constructor (temp: 30C, beeper: off)
  MideaFollowMe() : MideaData({MIDEA_TYPE_FOLLOW_ME, 0x82, 0x48, 0x7F, 0x1F}) {}
  // Copy from Base
  MideaFollowMe(const MideaData &data) : MideaData(data) {}
  // Direct from temperature and beeper values
  MideaFollowMe(uint8_t temp, bool beeper = false) : MideaFollowMe() {
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

#if 0
class MideaCommand : public MideaData {
 public:
  // Default constructor: power: on, mode: auto, fan: auto, temp: 25C, all timers: off
  MideaCommand() : MideaData({MIDEA_TYPE_COMMAND, 0x82, 0x48, 0xFF, 0xFF}) {}
  // Copy from Base
  MideaCommand(const MideaData &data) : MideaData(data) {}
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

template<typename... Ts> class MideaActionBase : public Action<Ts...> {
 public:
  void set_parent(MideaAC *parent) { this->parent_ = parent; }
  void transmit_ir(MideaData &data) { this->parent_->transmit_ir(data); }
  void transmit_ir(std::initializer_list<uint8_t> data) {
    MideaData code = data;
    this->parent_->transmit_ir(code);
  }

 protected:
  MideaAC *parent_;
};

template<typename... Ts> class FollowMeAction : public MideaActionBase<Ts...> {
  TEMPLATABLE_VALUE(uint8_t, temperature)
  TEMPLATABLE_VALUE(bool, beeper)

  void play(Ts... x) override {
    MideaFollowMe data;
    data.set_temp(this->temperature_.value(x...));
    data.set_beeper(this->beeper_.value(x...));
    this->transmit_ir(data);
  }
};

template<typename... Ts> class SwingStepAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->transmit_ir({MideaData::MIDEA_TYPE_SPECIAL, 0x01, 0xFF, 0xFF, 0xFF}); }
};

template<typename... Ts> class DisplayToggleAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->transmit_ir({MideaData::MIDEA_TYPE_SPECIAL, 0x08, 0xFF, 0xFF, 0xFF}); }
};

}  // namespace midea_ac
}  // namespace esphome
