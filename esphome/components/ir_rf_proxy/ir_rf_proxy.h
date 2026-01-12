#pragma once

#include "esphome/components/infrared/infrared.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_receiver/remote_receiver.h"

namespace esphome::ir_rf_proxy {

/// IrRfProxy - Infrared platform implementation using remote_transmitter/receiver as backend
class IrRfProxy : public infrared::Infrared {
 public:
  IrRfProxy() = default;

  void setup() override;
  void dump_config() override;

  /// Set RF frequency in kHz (0 = infrared, non-zero = RF)
  void set_frequency(uint32_t frequency_khz) { this->frequency_khz_ = frequency_khz; }
  /// Get RF frequency in kHz
  uint32_t get_frequency() const { return this->frequency_khz_; }
  /// Check if this is RF mode (non-zero frequency)
  bool is_rf() const { return this->frequency_khz_ > 0; }

  /// Set the remote_transmitter backend
  void set_remote_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter);
  /// Set the remote_receiver backend
  void set_remote_receiver(remote_receiver::RemoteReceiverComponent *receiver);

 protected:
  /// Implement the control method from infrared::Infrared base class
  void control(const infrared::InfraredCall &call) override;

  // RF frequency in kHz (Hz / 1000); 0 = infrared, non-zero = RF
  uint32_t frequency_khz_{0};
};

}  // namespace esphome::ir_rf_proxy
