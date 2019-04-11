#pragma once

#include "esphome/core/component.h"

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
  void set_inverted(bool inverted);

#ifdef USE_POWER_SUPPLY
  /** Use this to connect up a power supply to this output.
   *
   * Whenever this output is enabled, the power supply will automatically be turned on.
   *
   * @param power_supply The PowerSupplyComponent, set this to nullptr to disable the power supply.
   */
  void set_power_supply(PowerSupplyComponent *power_supply);
#endif

  /// Enable this binary output.
  virtual void turn_on();

  /// Disable this binary output.
  virtual void turn_off();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Return whether this binary output is inverted.
  bool is_inverted() const;

 protected:
  virtual void write_state(bool state) = 0;

  bool inverted_{false};
#ifdef USE_POWER_SUPPLY
  PowerSupplyComponent *power_supply_{nullptr};
  bool has_requested_high_power_{false};
#endif
};

}  // namespace output
}  // namespace esphome
