#include "m5pm1_binary_sensor.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::m5pm1 {

ESPHOME_LOG_TAG(TAG, "m5pm1.binary_sensor");
static constexpr uint32_t BUTTON_POLL_INTERVAL_MS = 50;

void M5PM1PowerButtonBinarySensor::setup() {
  // Publish an initial state after the hub has completed its IO-priority setup.
  this->update_state_();
  this->last_poll_ms_ = App.get_loop_component_start_time();
}

void M5PM1PowerButtonBinarySensor::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ms_ < BUTTON_POLL_INTERVAL_MS) {
    return;
  }
  this->last_poll_ms_ = now;
  this->update_state_();
}

void M5PM1PowerButtonBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "M5PM1 Power Button", this); }

void M5PM1PowerButtonBinarySensor::update_state_() {
  bool pressed;
  bool pressed_since_last_read;
  if (!this->parent_->get_button_state(&pressed, &pressed_since_last_read)) {
    this->status_set_warning(LOG_STR("Unable to read power button state"));
    return;
  }

  // BTN_Event catches a short press that begins and ends between two polls; reading it also clears it.
  this->publish_state(pressed || pressed_since_last_read);
  this->status_clear_warning();
}

}  // namespace esphome::m5pm1
