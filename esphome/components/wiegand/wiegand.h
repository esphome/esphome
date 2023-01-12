#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace wiegand {

class Wiegand;

struct WiegandStore {
  ISRInternalGPIOPin d0;
  ISRInternalGPIOPin d1;
  volatile uint64_t value{0};
  volatile uint32_t last_bit_time{0};
  volatile bool done{true};
  volatile uint8_t count{0};

  static void d0_gpio_intr(WiegandStore *arg);
  static void d1_gpio_intr(WiegandStore *arg);
};

class WiegandTagTrigger : public Trigger<std::string> {};

class WiegandRawTrigger : public Trigger<uint8_t, uint64_t> {};

class WiegandKeyTrigger : public Trigger<uint8_t> {};

class Wiegand : public key_provider::KeyProvider, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_d0_pin(InternalGPIOPin *pin) { this->d0_pin_ = pin; };
  void set_d1_pin(InternalGPIOPin *pin) { this->d1_pin_ = pin; };
  void register_tag_trigger(WiegandTagTrigger *trig) { this->tag_triggers_.push_back(trig); }
  void register_raw_trigger(WiegandRawTrigger *trig) { this->raw_triggers_.push_back(trig); }
  void register_key_trigger(WiegandKeyTrigger *trig) { this->key_triggers_.push_back(trig); }

 protected:
  InternalGPIOPin *d0_pin_;
  InternalGPIOPin *d1_pin_;
  WiegandStore store_{};
  std::vector<WiegandTagTrigger *> tag_triggers_;
  std::vector<WiegandRawTrigger *> raw_triggers_;
  std::vector<WiegandKeyTrigger *> key_triggers_;
};

}  // namespace wiegand
}  // namespace esphome
