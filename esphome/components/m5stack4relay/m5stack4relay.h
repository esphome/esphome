#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5stack4relay {

static constexpr uint8_t UNIT_4RELAY_CONFIG_REG = 0X10;
static constexpr uint8_t UNIT_4RELAY_RELAY_REG = 0X11;

enum class RelayBit : uint8_t {
  LED1 = 7,
  LED2 = 6,
  LED3 = 5,
  LED4 = 4,
  RELAY1 = 3,
  RELAY2 = 2,
  RELAY3 = 1,
  RELAY4 = 0,
  NONE = 255
};

class M5Relay4Switch;

// ====================================================================================

class M5Relay4Control : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  bool set_relay(RelayBit bit, bool state);
  void set_sync_mode(bool mode);

 protected:
  uint8_t get_relay_states_();
  bool has_been_setup_{false};
  bool sync_mode_{true};
};

// ====================================================================================

class M5Relay4Switch : public switch_::Switch, public Component, public Parented<M5Relay4Control> {
 public:
  M5Relay4Switch(RelayBit bit) : bit_(bit) {}

  void set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; };

 protected:
  bool assumed_state() override { return this->assumed_state_; };

  void write_state(bool state) override;

  bool assumed_state_{false};

  RelayBit bit_{RelayBit::NONE};
};

}  // namespace m5stack4relay
}  // namespace esphome
