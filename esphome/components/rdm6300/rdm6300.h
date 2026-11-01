#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>
#include <vector>

namespace esphome::rdm6300 {

class RDM6300BinarySensor;
class RDM6300Trigger;

class RDM6300Component final : public Component, public uart::UARTDevice {
 public:
  void loop() override;

#ifdef RDM6300_CARD_COUNT
  SLOT_LIST(RDM6300BinarySensor, cards_, register_card, RDM6300_CARD_COUNT)
#else
  SLOT_LIST_EMPTY(RDM6300BinarySensor, cards_)
#endif
#ifdef RDM6300_TRIGGER_COUNT
  SLOT_LIST(RDM6300Trigger, triggers_, register_trigger, RDM6300_TRIGGER_COUNT)
#else
  SLOT_LIST_EMPTY(RDM6300Trigger, triggers_)
#endif

 protected:
  int8_t read_state_{-1};
  uint8_t buffer_[6]{};
  uint32_t last_id_{0};
};

class RDM6300BinarySensor final : public binary_sensor::BinarySensorInitiallyOff {
 public:
  void set_id(uint32_t id) { id_ = id; }

  bool process(uint32_t id) {
    if (this->id_ == id) {
      this->publish_state(true);
      yield();
      this->publish_state(false);
      return true;
    }
    return false;
  }

 protected:
  uint32_t id_;
};

class RDM6300Trigger final : public Trigger<uint32_t> {
 public:
  void process(uint32_t uid) { this->trigger(uid); }
};

}  // namespace esphome::rdm6300
