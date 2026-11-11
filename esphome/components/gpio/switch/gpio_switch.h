#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"

namespace esphome::gpio {

class GPIOSwitch final : public switch_::Switch, public Component {
 public:
  // User provided, not "= default": `new(p) GPIOSwitch()` would zero-fill .bss that is already zero.
  GPIOSwitch() {}

  void set_pin(GPIOPin *pin) { pin_ = pin; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;
#ifdef USE_GPIO_SWITCH_INTERLOCK
  /// Codegen only: the table must outlive this switch; it may list this switch, which is skipped.
  void set_interlock(Switch *const *interlock, size_t count) {
    this->interlock_ = ConstVector<Switch *>(interlock, count);
  }
  void set_interlock_wait_time(uint32_t interlock_wait_time) { interlock_wait_time_ = interlock_wait_time; }
#endif

 protected:
  void write_state(bool state) override;

  GPIOPin *pin_{nullptr};
#ifdef USE_GPIO_SWITCH_INTERLOCK
  ConstVector<Switch *> interlock_;
  uint32_t interlock_wait_time_{0};
#endif
};

}  // namespace esphome::gpio
