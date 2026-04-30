#pragma once

#include "esphome/core/component.h"

#include "esphome/components/remote_base/remote_base.h"

#ifdef USE_RADIO_FREQUENCY
#include "esphome/components/radio_frequency/radio_frequency.h"
#endif

namespace esphome::remote_transmitter {

#ifdef USE_RADIO_FREQUENCY
class TransmitterRadioFrequency : public radio_frequency::RadioFrequency {
 public:
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }

  void setup() override;

 protected:
  void control(const radio_frequency::RadioFrequencyCall &call) override;

  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
};
#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::remote_transmitter
