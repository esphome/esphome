#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

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

class WiegandTagTrigger final : public Trigger<std::string> {};

class WiegandRawTrigger final : public Trigger<uint8_t, uint64_t> {};

class WiegandKeyTrigger final : public Trigger<uint8_t> {};

class Wiegand final : public key_provider::KeyProvider, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_d0_pin(InternalGPIOPin *pin) { this->d0_pin_ = pin; };
  void set_d1_pin(InternalGPIOPin *pin) { this->d1_pin_ = pin; };
#ifdef WIEGAND_TAG_TRIGGER_COUNT
  SLOT_LIST(WiegandTagTrigger, tag_triggers_, register_tag_trigger, WIEGAND_TAG_TRIGGER_COUNT)
#else
  SLOT_LIST_EMPTY(WiegandTagTrigger, tag_triggers_)
#endif
#ifdef WIEGAND_RAW_TRIGGER_COUNT
  SLOT_LIST(WiegandRawTrigger, raw_triggers_, register_raw_trigger, WIEGAND_RAW_TRIGGER_COUNT)
#else
  SLOT_LIST_EMPTY(WiegandRawTrigger, raw_triggers_)
#endif
#ifdef WIEGAND_KEY_TRIGGER_COUNT
  SLOT_LIST(WiegandKeyTrigger, key_triggers_, register_key_trigger, WIEGAND_KEY_TRIGGER_COUNT)
#else
  SLOT_LIST_EMPTY(WiegandKeyTrigger, key_triggers_)
#endif

 protected:
  InternalGPIOPin *d0_pin_;
  InternalGPIOPin *d1_pin_;
  WiegandStore store_{};
};

}  // namespace esphome::wiegand
