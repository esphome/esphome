#include "m5pm1_event.h"

#include "esphome/core/log.h"

namespace esphome::m5pm1 {

static const char *const TAG = "m5pm1.event";

M5PM1Event::M5PM1Event(M5PM1Component *parent, M5PM1EventType type, uint8_t enabled_mask, uint8_t enabled_wake_mask,
                       bool publish_generic_wakeup)
    : parent_(parent),
      type_(type),
      enabled_mask_(enabled_mask),
      enabled_wake_mask_(enabled_wake_mask),
      publish_generic_wakeup_(publish_generic_wakeup) {
  if (this->type_ == M5PM1EventType::M5PM1_EVENT_TYPE_POWER) {
    this->parent_->set_power_interrupt_enabled(this->enabled_mask_);
    this->parent_->add_on_power_interrupt_callback([this](uint8_t status) { this->process_power_interrupt_(status); });
  } else {
    if (this->type_ == M5PM1EventType::M5PM1_EVENT_TYPE_SYSTEM) {
      this->parent_->set_system_interrupt_enabled(this->enabled_mask_);
    }
    this->parent_->add_on_system_interrupt_callback(
        [this](uint8_t status, uint8_t wake_source) { this->process_system_interrupt_(status, wake_source); });
  }
}

void M5PM1Event::dump_config() {
  if (this->type_ == M5PM1EventType::M5PM1_EVENT_TYPE_POWER) {
    LOG_EVENT("", "M5PM1 Power Event", this);
  } else if (this->type_ == M5PM1EventType::M5PM1_EVENT_TYPE_SYSTEM) {
    LOG_EVENT("", "M5PM1 System Event", this);
  } else {
    LOG_EVENT("", "M5PM1 User Button Event", this);
  }
}

void M5PM1Event::process_power_interrupt_(uint8_t status) {
  static constexpr const char *EVENT_TYPES[] = {"vin_inserted",   "vin_removed",      "vinout_inserted",
                                                "vinout_removed", "battery_inserted", "battery_removed"};
  const uint8_t events = status & this->enabled_mask_;
  for (uint8_t bit = 0; bit < 6; bit++) {
    if ((events & (1 << bit)) != 0) {
      this->trigger(EVENT_TYPES[bit]);
    }
  }
}

void M5PM1Event::process_system_interrupt_(uint8_t status, uint8_t wake_source) {
  const uint8_t events = status & this->enabled_mask_;
  if ((events & M5PM1_IRQ_SYSTEM_SINGLE_CLICK) != 0) {
    this->trigger("single_click");
  }
  if ((events & M5PM1_IRQ_SYSTEM_WAKEUP) != 0) {
    static constexpr const char *WAKE_EVENT_TYPES[] = {
        "timer_wakeup",         "vin_wakeup",  "power_button_wakeup", "reset_button_wakeup",
        "command_reset_wakeup", "gpio_wakeup", "vinout_wakeup",
    };
    ESP_LOGD(TAG, "Wakeup event source mask: 0x%02X", wake_source);
    if (this->publish_generic_wakeup_) {
      this->trigger("wakeup");
    }
    const uint8_t enabled_sources = wake_source & this->enabled_wake_mask_;
    for (uint8_t bit = 0; bit < 7; bit++) {
      if ((enabled_sources & (1 << bit)) != 0) {
        this->trigger(WAKE_EVENT_TYPES[bit]);
      }
    }
  }
  if ((events & M5PM1_IRQ_SYSTEM_DOUBLE_CLICK) != 0) {
    this->trigger("double_click");
  }
}

}  // namespace esphome::m5pm1
