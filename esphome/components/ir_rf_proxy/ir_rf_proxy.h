#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/components/remote_base/remote_base.h"

#ifdef USE_IR_RF
#include "esphome/components/infrared/infrared.h"
#endif

namespace esphome::ir_rf_proxy {

/// Transmit raw timings via a remote_transmitter backend.
/// Works with any call type that exposes the raw-timings interface
/// (InfraredCall, RadioFrequencyCall, or any future call type with the same methods).
template<typename CallT>
void transmit_raw_timings(remote_base::RemoteTransmitterBase *transmitter, uint32_t carrier_frequency,
                          const CallT &call);

#ifdef USE_IR_RF
/// IrRfProxy - Infrared platform implementation using remote_transmitter/receiver as backend
class IrRfProxy : public infrared::Infrared {
 public:
  IrRfProxy() = default;

  void dump_config() override;

  /// Set RF frequency in kHz (0 = infrared, non-zero = RF)
  void set_frequency(uint32_t frequency_khz) { this->frequency_khz_ = frequency_khz; }
  /// Get RF frequency in kHz
  uint32_t get_frequency() const { return this->frequency_khz_; }
  /// Check if this is RF mode (non-zero frequency)
  bool is_rf() const { return this->frequency_khz_ > 0; }

  /// Set the receiver's hardware demodulation frequency in Hz (metadata only, does not affect hardware)
  void set_receiver_frequency(uint32_t frequency_hz) { this->get_traits().set_receiver_frequency_hz(frequency_hz); }

 protected:
  // RF frequency in kHz (Hz / 1000); 0 = infrared, non-zero = RF
  uint32_t frequency_khz_{0};
};
#endif  // USE_IR_RF

}  // namespace esphome::ir_rf_proxy
