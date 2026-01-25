#pragma once

#include "esphome/components/ds248x_base/ds248x_base.h"

namespace esphome {
namespace ds2482 {

/// DS2482-800 8-channel I2C-to-1-Wire bridge
/// Based on Analog Devices DS2482-800 datasheet:
/// https://www.analog.com/media/en/technical-documentation/data-sheets/ds2482-800.pdf
class DS2482OneWireBus : public ds248x_base::DS248xOneWireBusBase {
 public:
  void setup() override;
  void dump_config() override;

  /// Set which channel (0-7) this bus instance uses
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  /// Hook called before each 1-Wire operation to select the correct channel
  bool pre_operation_hook_() override;

  /// Hook called after device reset to invalidate channel cache
  void post_reset_hook_() override;

  /// Detect whether this is a DS2482-800 (8-channel) or DS2482-100 (single-channel)
  /// @return true if DS2482-800, false if DS2482-100
  bool detect_variant_();

  /// Select the configured channel on the DS2482
  /// @return true on success, false on I2C error or verification failure
  bool select_channel_();

  uint8_t channel_{0};         ///< Configured channel (0-7)
  bool is_ds2482_800_{false};  ///< true if DS2482-800, false if DS2482-100
};

}  // namespace ds2482
}  // namespace esphome
