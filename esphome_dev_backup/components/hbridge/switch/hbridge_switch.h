#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"

#include <vector>

namespace esphome {
namespace hbridge {

enum RelayState : uint8_t {
  RELAY_STATE_OFF = 0,
  RELAY_STATE_ON = 1,
  RELAY_STATE_SWITCHING_ON = 2,
  RELAY_STATE_SWITCHING_OFF = 3,
  RELAY_STATE_UNKNOWN = 4,
};

class HBridgeSwitch : public switch_::Switch, public Component {
 public:
  void set_on_pin(GPIOPin *pin) { this->on_pin_ = pin; }
  void set_off_pin(GPIOPin *pin) { this->off_pin_ = pin; }
  void set_pulse_length(uint32_t pulse_length) { this->pulse_length_ = pulse_length; }
  void set_wait_time(uint32_t wait_time) { this->wait_time_ = wait_time; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  void timer_fn_();

  bool timer_running_{false};
  bool desired_state_{false};
  RelayState relay_state_{RELAY_STATE_UNKNOWN};
  GPIOPin *on_pin_{nullptr};
  GPIOPin *off_pin_{nullptr};
  uint32_t pulse_length_{0};
  uint32_t wait_time_{0};
  bool optimistic_{false};
};

}  // namespace hbridge
}  // namespace esphome
