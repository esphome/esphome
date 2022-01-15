#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include <IRSender.h>  // arduino-heatpump library

namespace esphome {
namespace heatpumpir {

class IRSenderESPHome : public IRSender {
 public:
  IRSenderESPHome(remote_transmitter::RemoteTransmitterComponent *transmitter)
      : IRSender(0), transmit_(transmitter->transmit()){};
  void setFrequency(int frequency) override;  // NOLINT(readability-identifier-naming)
  void space(int space_length) override;
  void mark(int mark_length) override;

 protected:
  remote_transmitter::RemoteTransmitterComponent::TransmitCall transmit_;
};

}  // namespace heatpumpir
}  // namespace esphome

#endif
