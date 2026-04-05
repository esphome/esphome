#pragma once
#include "esphome/core/automation.h"
#include "nextion.h"

namespace esphome::nextion {

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

}  // namespace esphome::nextion
