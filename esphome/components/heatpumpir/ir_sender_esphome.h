#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include <IRSender.h>  // arduino-heatpump library

namespace esphome {
namespace heatpumpir {

class IRSenderESPHome : public IRSender {
 public:
  IRSenderESPHome(uint8_t pin, remote_transmitter::RemoteTransmitterComponent *transmitter)
      : IRSender(pin), transmit_(transmitter->transmit()){};
  void setFrequency(int frequency);  // NOLINT(readability-identifier-naming)
  void space(int space_length);
  void mark(int mark_length);

 protected:
  remote_transmitter::RemoteTransmitterComponent::TransmitCall transmit_;
};

}  // namespace heatpumpir
}  // namespace esphome
