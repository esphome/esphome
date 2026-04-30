#pragma once

#include "esphome/core/component.h"
#include "esphome/components/radio_frequency/radio_frequency.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_transmitter {

class TransmitterRadioFrequency : public radio_frequency::RadioFrequency {
 public:
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }

  void setup() override;

 protected:
  void control(const radio_frequency::RadioFrequencyCall &call) override;

  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
};

}  // namespace esphome::remote_transmitter
