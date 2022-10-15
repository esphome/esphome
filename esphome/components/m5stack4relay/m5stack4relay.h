#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/switch.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5stack_4relay {

static constexpr uint8_t UNIT_4RELAY_CONFIG_REG = 0X10;
static constexpr uint8_t UNIT_4RELAY_RELAY_REG = 0X11;

enum class relay_bit: uitn8_t {
    LED0 = 7,
    LED1 = 6,
    LED2 = 5,
    LED3 = 4,
    RELAY0 = 3,
    RELAY1 = 2,
    RELAY2 = 1,
    RELAY3 = 0,
    NONE = 255
}

class M5Relay4Switch;

// ====================================================================================

class M5Relay4Control : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  bool set_relay(RELAY_BIT number, bool state);
  void set_sync_mode (bool mode);

 protected:
  uint8_t get_relay_states_();
  bool has_been_setup_{false};
  bool sync_mode_{true};
}

// ====================================================================================

class M5Relay4Switch : public switch_::Switch,
                       public Component,
                       public Parented<M5Relay4Control> {
 public:
  M5Relay4Switch(RELAY_BIT bit) : bit_(bit) {}

  void set_assumed_state(bool assumed_state){this->assumed_state_ = assumed_state};

 protected:
  bool assumed_state() override { return this->assumed_state_; };

  void write_state(bool state) override;

  bool assumed_state_{false};

  relay_bit bit_{relay_bit::NONE};
}

}  // namespace m5stack_4relay
}  // namespace esphome