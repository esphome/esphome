#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace output {

#define LOG_BINARY_OUTPUT(this) \
  if (this->inverted_) { \
    ESP_LOGCONFIG(TAG, "  Inverted: YES"); \
  }

class BinaryOutput {
 public:
  /// Set the inversion state of this binary output.
  void set_inverted(bool inverted) { this->inverted_ = inverted; }

#ifdef USE_POWER_SUPPLY
  /** Use this to connect up a power supply to this output.
   *
   * Whenever this output is enabled, the power supply will automatically be turned on.
   *
   * @param power_supply The PowerSupplyComponent, set this to nullptr to disable the power supply.
   */
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  /// Enable or disable this binary output.
  virtual void set_state(bool state) {
    if (state) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }

  /// Enable this binary output.
  virtual void turn_on() {
#ifdef USE_POWER_SUPPLY
    this->power_.request();
#endif
    this->write_state(!this->inverted_);
  }

  /// Disable this binary output.
  virtual void turn_off() {
#ifdef USE_POWER_SUPPLY
    this->power_.unrequest();
#endif
    this->write_state(this->inverted_);
  }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Return whether this binary output is inverted.
  bool is_inverted() const { return this->inverted_; }

 protected:
  virtual void write_state(bool state) = 0;

  bool inverted_{false};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_{};
#endif
};

}  // namespace output
}  // namespace esphome
