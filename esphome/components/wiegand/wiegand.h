#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::wiegand {

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

class Wiegand final : public key_provider::KeyProvider, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_d0_pin(InternalGPIOPin *pin) { this->d0_pin_ = pin; };
  void set_d1_pin(InternalGPIOPin *pin) { this->d1_pin_ = pin; };
#ifdef WIEGAND_ON_TAG_COUNT
  void add_on_tag_automation(Automation<std::string> *automation) { this->tag_automations_.push_back(automation); }
#endif
#ifdef WIEGAND_ON_RAW_COUNT
  void add_on_raw_automation(Automation<uint8_t, uint64_t> *automation) {
    this->raw_automations_.push_back(automation);
  }
#endif
#ifdef WIEGAND_ON_KEY_COUNT
  void add_on_key_automation(Automation<uint8_t> *automation) { this->key_automations_.push_back(automation); }
#endif

 protected:
  void trigger_tag_(const char *tag) {
#ifdef WIEGAND_ON_TAG_COUNT
    for (auto *automation : this->tag_automations_)
      automation->trigger(tag);
#endif
  }
  void trigger_key_(uint8_t key) {
#ifdef WIEGAND_ON_KEY_COUNT
    for (auto *automation : this->key_automations_)
      automation->trigger(key);
#endif
  }

  InternalGPIOPin *d0_pin_;
  InternalGPIOPin *d1_pin_;
  WiegandStore store_{};
#ifdef WIEGAND_ON_TAG_COUNT
  StaticVector<Automation<std::string> *, WIEGAND_ON_TAG_COUNT> tag_automations_;
#endif
#ifdef WIEGAND_ON_RAW_COUNT
  StaticVector<Automation<uint8_t, uint64_t> *, WIEGAND_ON_RAW_COUNT> raw_automations_;
#endif
#ifdef WIEGAND_ON_KEY_COUNT
  StaticVector<Automation<uint8_t> *, WIEGAND_ON_KEY_COUNT> key_automations_;
#endif
};

}  // namespace esphome::wiegand
