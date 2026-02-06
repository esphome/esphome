#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/components/infrared/infrared.h"

namespace esphome::ir_rf_proxy {

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

 protected:
  // RF frequency in kHz (Hz / 1000); 0 = infrared, non-zero = RF
  uint32_t frequency_khz_{0};
};

}  // namespace esphome::ir_rf_proxy
