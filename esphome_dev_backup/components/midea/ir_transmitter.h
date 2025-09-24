#pragma once

#ifdef USE_ARDUINO
#ifdef USE_REMOTE_TRANSMITTER
#include "esphome/components/remote_base/midea_protocol.h"

namespace esphome {
namespace midea {

using remote_base::RemoteTransmitterBase;
using IrData = remote_base::MideaData;

class IrFollowMeData : public IrData {
 public:
  // Default constructor (temp: 30C, beeper: off)
  IrFollowMeData() : IrData({MIDEA_TYPE_FOLLOW_ME, 0x82, 0x48, 0x7F, 0x1F}) {}
  // Copy from Base
  IrFollowMeData(const IrData &data) : IrData(data) {}
  // Direct from temperature in celsius and beeper values
  IrFollowMeData(uint8_t temp, bool beeper = false) : IrFollowMeData() {
    this->set_temp(temp, false);
    this->set_beeper(beeper);
  }
  // Direct from temperature, fahrenheit and beeper values
  IrFollowMeData(uint8_t temp, bool fahrenheit, bool beeper) : IrFollowMeData() {
    this->set_temp(temp, fahrenheit);
    this->set_beeper(beeper);
  }

  /* TEMPERATURE */
  uint8_t temp() const {
    if (this->fahrenheit()) {
      return this->get_value_(4) + 31;
    }
    return this->get_value_(4) - 1;
  }
  void set_temp(uint8_t val, bool fahrenheit = false) {
    this->set_fahrenheit(fahrenheit);
    if (this->fahrenheit()) {
      // see https://github.com/esphome/feature-requests/issues/1627#issuecomment-1365639966
      val = esphome::clamp<uint8_t>(val, MIN_TEMP_F, MAX_TEMP_F) - 31;
    } else {
      val = esphome::clamp<uint8_t>(val, MIN_TEMP_C, MAX_TEMP_C) + 1;
    }
    this->set_value_(4, val);
  }

  /* BEEPER */
  bool beeper() const { return this->get_value_(3, 128); }
  void set_beeper(bool val) { this->set_mask_(3, val, 128); }

  /* FAHRENHEIT */
  bool fahrenheit() const { return this->get_value_(2, 32); }
  void set_fahrenheit(bool val) { this->set_mask_(2, val, 32); }

 protected:
  static const uint8_t MIN_TEMP_C = 0;
  static const uint8_t MAX_TEMP_C = 37;

  // see
  // https://github.com/crankyoldgit/IRremoteESP8266/blob/9bdf8abcb465268c5409db99dc83a26df64c7445/src/ir_Midea.h#L116
  static const uint8_t MIN_TEMP_F = 32;
  // see
  // https://github.com/crankyoldgit/IRremoteESP8266/blob/9bdf8abcb465268c5409db99dc83a26df64c7445/src/ir_Midea.h#L117
  static const uint8_t MAX_TEMP_F = 99;
};

class IrSpecialData : public IrData {
 public:
  IrSpecialData(uint8_t code) : IrData({MIDEA_TYPE_SPECIAL, code, 0xFF, 0xFF, 0xFF}) {}
};

class IrTransmitter {
 public:
  void set_transmitter(RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }
  void transmit(IrData &data) {
    data.finalize();
    auto transmit = this->transmitter_->transmit();
    remote_base::MideaProtocol().encode(transmit.get_data(), data);
    transmit.perform();
  }

 protected:
  RemoteTransmitterBase *transmitter_{nullptr};
};

}  // namespace midea
}  // namespace esphome

#endif
#endif  // USE_ARDUINO
