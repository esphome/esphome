#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace power_supply {

class PowerSupply : public Component {
 public:
  explicit PowerSupply(GPIOPin *pin, uint32_t enable_time, uint32_t keep_on_time);

  /// Is this power supply currently on?
  bool is_enabled() const;

  /// Request high power mode. Use unrequest_high_power() to remove this request.
  void request_high_power();

  /// Un-request high power mode.
  void unrequest_high_power();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Register callbacks.
  void setup() override;
  void dump_config() override;
  /// Hardware setup priority (+1).
  float get_setup_priority() const override;

  void on_shutdown() override;

 protected:
  GPIOPin *pin_;
  bool enabled_{false};
  uint32_t enable_time_;
  uint32_t keep_on_time_;
  int16_t active_requests_{0};  // use signed integer to make catching negative requests easier.
};

}  // namespace power_supply
}  // namespace esphome
