#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>

namespace esphome::rdm6300 {

// A template on the sensor type keeps binary_sensor.h out of this hub header
template<typename T> inline bool publish_card(T *sensor, uint32_t tag, uint32_t card) {
  if (tag != card)
    return false;
  sensor->publish_state(true);
  yield();
  sensor->publish_state(false);
  return true;
}

class RDM6300Component final : public Component, public uart::UARTDevice {
 public:
  void loop() override;

#ifdef USE_RDM6300_CARDS_PUBLISHER
  HUB_PUBLISHER_RETURNING(bool, cards, uint32_t)
#endif
#ifdef RDM6300_ON_TAG_COUNT
  void add_on_tag_automation(Automation<uint32_t> *automation) { this->tag_automations_.add(automation); }
#endif

 protected:
  int8_t read_state_{-1};
  uint8_t buffer_[6]{};
#ifdef RDM6300_ON_TAG_COUNT
  AutomationList<RDM6300_ON_TAG_COUNT, uint32_t> tag_automations_;
#endif
  uint32_t last_id_{0};
};

}  // namespace esphome::rdm6300
