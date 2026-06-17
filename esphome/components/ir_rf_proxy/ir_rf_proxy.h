#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/components/remote_base/remote_base.h"

#ifdef USE_IR_RF
#include "esphome/components/infrared/infrared.h"
#endif

#ifdef USE_RADIO_FREQUENCY
#include "esphome/components/radio_frequency/radio_frequency.h"
#endif

namespace esphome::ir_rf_proxy {

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
  void control(const infrared::InfraredCall &call) override;

  // RF frequency in kHz (Hz / 1000); 0 = infrared, non-zero = RF
  uint32_t frequency_khz_{0};
};
#endif  // USE_IR_RF

#ifdef USE_RADIO_FREQUENCY
/// RfProxy - Radio Frequency platform implementation using remote_transmitter/receiver as backend.
/// Driver-agnostic: integration with specific RF front-end chips (CC1101, RFM69, etc.) is done
/// in YAML by wiring their actions to `remote_transmitter`'s on_transmit/on_complete triggers and
/// to this entity's on_control trigger (see radio_frequency component docs).
class RfProxy : public radio_frequency::RadioFrequency {
 public:
  RfProxy() = default;

  void setup() override;
  void dump_config() override;

  /// Set the remote transmitter component
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }
  /// Set the remote receiver component
  void set_receiver(remote_base::RemoteReceiverBase *receiver) { this->receiver_ = receiver; }

  /// Set the fixed carrier frequency in Hz (metadata: advertised via traits, does not tune hardware)
  void set_frequency_hz(uint32_t freq_hz) { this->traits_.set_fixed_frequency_hz(freq_hz); }

 protected:
  void control(const radio_frequency::RadioFrequencyCall &call) override;

  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
  remote_base::RemoteReceiverBase *receiver_{nullptr};
};
#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::ir_rf_proxy
