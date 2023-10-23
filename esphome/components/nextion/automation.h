#pragma once
#include "esphome/core/automation.h"
#include "nextion.h"

namespace esphome {
namespace nextion {

class SetupTrigger : public Trigger<> {
 public:
  explicit SetupTrigger(Nextion *nextion) {
    nextion->add_setup_state_callback([this]() { this->trigger(); });
  }
};

class SleepTrigger : public Trigger<> {
 public:
  explicit SleepTrigger(Nextion *nextion) {
    nextion->add_sleep_state_callback([this]() { this->trigger(); });
  }
};

class WakeTrigger : public Trigger<> {
 public:
  explicit WakeTrigger(Nextion *nextion) {
    nextion->add_wake_state_callback([this]() { this->trigger(); });
  }
};

class PageTrigger : public Trigger<uint8_t> {
 public:
  explicit PageTrigger(Nextion *nextion) {
    nextion->add_new_page_callback([this](const uint8_t page_id) { this->trigger(page_id); });
  }
};

}  // namespace nextion
}  // namespace esphome
