#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace ds248x_base {

/// Base class for DS248x I2C-to-1-Wire bridge chips (DS2484, DS2482)
/// Provides shared implementation for common operations
class DS248xOneWireBusBase : public one_wire::OneWireBus, public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS - 1.0; }

  /// Reset the DS248x device and configure pullup settings
  bool reset_device();

  // OneWireBus interface implementation
  int reset_int() override;
  void write8(uint8_t val) override;
  void write64(uint64_t val) override;
  uint8_t read8() override;
  uint64_t read64() override;

  /// Set active pullup configuration (1.5kΩ pullup)
  void set_active_pullup(bool value) { this->active_pullup_ = value; }
  /// Set strong pullup configuration (for power delivery to parasitic devices)
  void set_strong_pullup(bool value) { this->strong_pullup_ = value; }

 protected:
  // OneWireBus virtual methods
  void reset_search() override;
  uint64_t search_int() override;

  /// Read status byte from DS248x with retry logic
  bool read_status_(uint8_t *status);
  /// Wait for DS248x to complete current operation
  bool wait_for_completion_();
  /// Internal write8 implementation (no channel selection)
  void write8_(uint8_t value);
  /// Perform 1-Wire triplet operation (used in search algorithm)
  bool one_wire_triple_(bool *branch, bool *id_bit, bool *cmp_id_bit);

  /// Hook called before each 1-Wire operation
  /// DS2482 uses this for channel selection, DS2484 does nothing
  /// @return true if operation can proceed, false on error
  virtual bool pre_operation_hook_() { return true; }

  /// Hook called after device reset
  /// DS2482 uses this to invalidate channel cache, DS2484 does nothing
  virtual void post_reset_hook_() {}

  // Search state (standard 1-Wire ROM search algorithm)
  uint64_t address_;              ///< Current device address being searched
  uint8_t last_discrepancy_{0};   ///< Last bit position where devices diverged
  bool last_device_flag_{false};  ///< Set when last device has been found

  // Configuration
  bool active_pullup_{false};  ///< Enable active pullup (1.5kΩ)
  bool strong_pullup_{false};  ///< Enable strong pullup (power delivery)
};

}  // namespace ds248x_base
}  // namespace esphome
