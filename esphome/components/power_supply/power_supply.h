#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <cinttypes>

namespace esphome {
namespace power_supply {

class PowerSupply : public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_enable_time(uint32_t enable_time) { enable_time_ = enable_time; }
  void set_keep_on_time(uint32_t keep_on_time) { keep_on_time_ = keep_on_time; }
  void set_enable_on_boot(bool enable_on_boot) { enable_on_boot_ = enable_on_boot; }

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
  bool enable_on_boot_{false};
  uint32_t enable_time_;
  uint32_t keep_on_time_;
  int16_t active_requests_{0};  // use signed integer to make catching negative requests easier.
};

class PowerSupplyRequester {
 public:
  void set_parent(PowerSupply *parent) { parent_ = parent; }
  void request() {
    if (!this->requested_ && this->parent_ != nullptr) {
      this->parent_->request_high_power();
      this->requested_ = true;
    }
  }
  void unrequest() {
    if (this->requested_ && this->parent_ != nullptr) {
      this->parent_->unrequest_high_power();
      this->requested_ = false;
    }
  }

 protected:
  PowerSupply *parent_{nullptr};
  bool requested_{false};
};

}  // namespace power_supply
}  // namespace esphome
