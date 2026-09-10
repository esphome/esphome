#pragma once

#include "esphome/core/component.h"
#include "esphome/components/one_wire/one_wire_bus.h"

namespace esphome::ds248x {

class DS248xComponent;

/**
 * @brief OneWireBus implementation for DS248x I2C-to-1-Wire bridges.
 *
 * This class wraps the DS248xComponent to provide the one_wire::OneWireBus interface,
 * enabling compatibility with all existing 1-Wire device components (dallas_temp, etc.).
 *
 * For DS2482-800, multiple instances of this class can be created (one per channel).
 * For DS2482-100/DS2484, a single instance is used.
 */
class DS248xOneWireBus : public one_wire::OneWireBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

  /// Set the parent DS248x component
  void set_parent(DS248xComponent *parent) { this->parent_ = parent; }

  /// Set the 1-Wire channel (0-7, only relevant for DS2482-800)
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  /// Get the channel number
  uint8_t get_channel() const { return this->channel_; }

  // OneWireBus interface implementation
  int reset_int() override;
  void write8(uint8_t val) override;
  void write64(uint64_t val) override;
  uint8_t read8() override;
  uint64_t read64() override;

 protected:
  void reset_search() override;
  uint64_t search_int() override;

  /// Select the channel on the DS248x before any 1-Wire operation
  bool ensure_channel_();

  DS248xComponent *parent_{nullptr};
  uint8_t channel_{0};

  // Search state
  uint64_t search_address_{0};
  uint8_t search_last_discrepancy_{0};
  bool search_last_device_flag_{false};
};

}  // namespace esphome::ds248x
