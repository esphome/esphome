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
  // Direct from temperature and beeper values
  IrFollowMeData(uint8_t temp, bool beeper = false) : IrFollowMeData() {
    this->set_temp(temp);
    this->set_beeper(beeper);
  }

  /* TEMPERATURE */
  uint8_t temp() const { return this->get_value_(4) - 1; }
  void set_temp(uint8_t val) { this->set_value_(4, std::min(MAX_TEMP, val) + 1); }

  /* BEEPER */
  bool beeper() const { return this->get_value_(3, 128); }
  void set_beeper(bool val) { this->set_mask_(3, val, 128); }

 protected:
  static const uint8_t MAX_TEMP = 37;
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
