#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"

namespace esphome {
namespace climate {

class ClimateRemoteTransmitter : public climate::Climate, public Component {
 public:
  ClimateRemoteTransmitter(const std::string &name);
  void setup() override;

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter);

  void set_supports_cool(bool supports_cool);
  void set_supports_heat(bool supports_heat);

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Transmit via IR the state of this climate controller.
  void transmit_state_();

  void sendData(remote_base::RemoteTransmitData * transmitData, uint16_t onemark, uint32_t onespace, uint16_t zeromark,
                uint32_t zerospace, uint64_t data, uint16_t nbits,
                bool MSBfirst = true);

  bool supports_cool_{true};
  bool supports_heat_{true};
  
  remote_transmitter::RemoteTransmitterComponent *transmitter_;
};

}  // namespace climate
}  // namespace esphome
