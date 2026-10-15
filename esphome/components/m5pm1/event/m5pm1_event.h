#pragma once

#include <cstdint>

#include "esphome/components/event/event.h"
#include "esphome/core/component.h"

#include "../m5pm1.h"

namespace esphome::m5pm1 {

enum class M5PM1EventType : uint8_t {
  M5PM1_EVENT_TYPE_POWER,
  M5PM1_EVENT_TYPE_SYSTEM,
  M5PM1_EVENT_TYPE_USER_BUTTON,
};

class M5PM1Event : public event::Event, public Component {
 public:
  M5PM1Event(M5PM1Component *parent, M5PM1EventType type, uint8_t enabled_mask, uint8_t enabled_wake_mask,
             bool publish_generic_wakeup);

  void dump_config() override;

 protected:
  void process_power_interrupt_(uint8_t status);
  void process_system_interrupt_(uint8_t status, uint8_t wake_source);

  M5PM1Component *parent_;
  M5PM1EventType type_;
  uint8_t enabled_mask_;
  uint8_t enabled_wake_mask_;
  bool publish_generic_wakeup_;
};

}  // namespace esphome::m5pm1
