#include "esphome/core/automation.h"

namespace esphome {

static const char *TAG = "automation";

void StartupTrigger::setup() { this->trigger(); }
float StartupTrigger::get_setup_priority() const {
  // Run after everything is set up
  return this->setup_priority_;
}
StartupTrigger::StartupTrigger(float setup_priority) : setup_priority_(setup_priority) {}

void ShutdownTrigger::on_shutdown() {
  this->trigger();
}

void LoopTrigger::loop() { this->trigger(); }
float LoopTrigger::get_setup_priority() const { return setup_priority::DATA; }

void IntervalTrigger::update() { this->trigger(); }
float IntervalTrigger::get_setup_priority() const { return setup_priority::DATA; }

IntervalTrigger::IntervalTrigger(uint32_t update_interval) : PollingComponent(update_interval) {}

RangeCondition::RangeCondition() = default;

bool RangeCondition::check(float x) {
  float min = this->min_.value(x);
  float max = this->max_.value(x);
  if (isnan(min)) {
    return x >= max;
  } else if (isnan(max)) {
    return x >= min;
  } else {
    return min <= x && x <= max;
  }
}

void Script::execute() { this->trigger(); }

}  // namespace esphome
