#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/string_ref.h"

#include "nextion.h"

namespace esphome {
namespace nextion {

class BufferOverflowTrigger : public Trigger<> {
 public:
  explicit BufferOverflowTrigger(Nextion *nextion) {
    nextion->add_buffer_overflow_event_callback([this]() { this->trigger(); });
  }
};

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

class TouchTrigger : public Trigger<uint8_t, uint8_t, bool> {
 public:
  explicit TouchTrigger(Nextion *nextion) {
    nextion->add_touch_event_callback([this](uint8_t page_id, uint8_t component_id, bool touch_event) {
      this->trigger(page_id, component_id, touch_event);
    });
  }
};

#ifdef USE_NEXTION_TRIGGER_CUSTOM_BINARY_SENSOR
class CustomBinarySensorTrigger : public Trigger<StringRef, bool> {
 public:
  explicit CustomBinarySensorTrigger(Nextion *nextion) {
    nextion->add_custom_binary_sensor_callback([this](const StringRef &key, bool value) { this->trigger(key, value); });
  }
};
#endif  // USE_NEXTION_TRIGGER_CUSTOM_BINARY_SENSOR

#ifdef USE_NEXTION_TRIGGER_CUSTOM_SENSOR
class CustomSensorTrigger : public Trigger<StringRef, int32_t> {
 public:
  explicit CustomSensorTrigger(Nextion *nextion) {
    nextion->add_custom_sensor_callback([this](const StringRef &key, int32_t value) { this->trigger(key, value); });
  }
};
#endif  // USE_NEXTION_TRIGGER_CUSTOM_SENSOR

#ifdef USE_NEXTION_TRIGGER_CUSTOM_SWITCH
class CustomSwitchTrigger : public Trigger<StringRef, bool> {
 public:
  explicit CustomSwitchTrigger(Nextion *nextion) {
    nextion->add_custom_switch_callback([this](const StringRef &key, bool value) { this->trigger(key, value); });
  }
};
#endif  // USE_NEXTION_TRIGGER_CUSTOM_SWITCH

#ifdef USE_NEXTION_TRIGGER_CUSTOM_TEXT_SENSOR
class CustomTextSensorTrigger : public Trigger<StringRef, StringRef> {
 public:
  explicit CustomTextSensorTrigger(Nextion *nextion) {
    nextion->add_custom_text_sensor_callback(
        [this](const StringRef &key, const StringRef &value) { this->trigger(key, value); });
  }
};
#endif  // USE_NEXTION_TRIGGER_CUSTOM_TEXT_SENSOR

template<typename... Ts> class NextionSetBrightnessAction : public Action<Ts...> {
 public:
  explicit NextionSetBrightnessAction(Nextion *component) : component_(component) {}

  TEMPLATABLE_VALUE(float, brightness)

  void play(const Ts &...x) override {
    this->component_->set_brightness(this->brightness_.value(x...));
    this->component_->set_backlight_brightness(this->brightness_.value(x...));
  }

  void set_brightness(std::function<void(Ts..., float)> brightness) { this->brightness_ = brightness; }

 protected:
  Nextion *component_;
};

template<typename... Ts> class NextionPublishFloatAction : public Action<Ts...> {
 public:
  explicit NextionPublishFloatAction(NextionComponent *component) : component_(component) {}

  TEMPLATABLE_VALUE(float, state)
  TEMPLATABLE_VALUE(bool, publish_state)
  TEMPLATABLE_VALUE(bool, send_to_nextion)

  void play(const Ts &...x) override {
    this->component_->set_state(this->state_.value(x...), this->publish_state_.value(x...),
                                this->send_to_nextion_.value(x...));
  }

  void set_state(std::function<void(Ts..., float)> state) { this->state_ = state; }
  void set_publish_state(std::function<void(Ts..., bool)> publish_state) { this->publish_state_ = publish_state; }
  void set_send_to_nextion(std::function<void(Ts..., bool)> send_to_nextion) {
    this->send_to_nextion_ = send_to_nextion;
  }

 protected:
  NextionComponent *component_;
};

template<typename... Ts> class NextionPublishTextAction : public Action<Ts...> {
 public:
  explicit NextionPublishTextAction(NextionComponent *component) : component_(component) {}

  TEMPLATABLE_VALUE(const char *, state)
  TEMPLATABLE_VALUE(bool, publish_state)
  TEMPLATABLE_VALUE(bool, send_to_nextion)

  void play(const Ts &...x) override {
    this->component_->set_state(this->state_.value(x...), this->publish_state_.value(x...),
                                this->send_to_nextion_.value(x...));
  }

  void set_state(std::function<void(Ts..., const char *)> state) { this->state_ = state; }
  void set_publish_state(std::function<void(Ts..., bool)> publish_state) { this->publish_state_ = publish_state; }
  void set_send_to_nextion(std::function<void(Ts..., bool)> send_to_nextion) {
    this->send_to_nextion_ = send_to_nextion;
  }

 protected:
  NextionComponent *component_;
};

template<typename... Ts> class NextionPublishBoolAction : public Action<Ts...> {
 public:
  explicit NextionPublishBoolAction(NextionComponent *component) : component_(component) {}

  TEMPLATABLE_VALUE(bool, state)
  TEMPLATABLE_VALUE(bool, publish_state)
  TEMPLATABLE_VALUE(bool, send_to_nextion)

  void play(const Ts &...x) override {
    this->component_->set_state(this->state_.value(x...), this->publish_state_.value(x...),
                                this->send_to_nextion_.value(x...));
  }

  void set_state(std::function<void(Ts..., bool)> state) { this->state_ = state; }
  void set_publish_state(std::function<void(Ts..., bool)> publish_state) { this->publish_state_ = publish_state; }
  void set_send_to_nextion(std::function<void(Ts..., bool)> send_to_nextion) {
    this->send_to_nextion_ = send_to_nextion;
  }

 protected:
  NextionComponent *component_;
};

}  // namespace nextion
}  // namespace esphome
