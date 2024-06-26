#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"

#include <vector>

namespace esphome {
namespace gpio {

class GPIOSwitch : public switch_::Switch, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;
  void set_interlock(const std::vector<Switch *> &interlock);
  void set_interlock_wait_time(uint32_t interlock_wait_time) { interlock_wait_time_ = interlock_wait_time; }

 protected:
  void write_state(bool state) override;

  GPIOPin *pin_;
  std::vector<Switch *> interlock_;
  uint32_t interlock_wait_time_{0};
};

}  // namespace gpio
}  // namespace esphome
