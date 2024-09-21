#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  void update() override {
    if (this->started_)
      this->trigger();
  }

  void setup() override {
    if (!this->autostart_) {
      return;
    }
    if (this->startup_delay_ == 0) {
      this->started_ = true;
    } else {
      this->set_timeout(this->startup_delay_, [this] { this->start(); });
    }
  }

  void set_startup_delay(const uint32_t startup_delay) { this->startup_delay_ = startup_delay; }

  void set_autostart(const bool autostart) { this->autostart_ = autostart; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void start() { this->started_ = true; }
  void stop() { this->started_ = false; }

 protected:
  uint32_t startup_delay_{0};
  bool autostart_{true};
  bool started_{false};
};

template<typename... Ts> class IntervalStartAction : public Action<Ts...> {
 public:
  IntervalStartAction(IntervalTrigger *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(const char *, url)
  void play(Ts... x) override { this->parent_->start(); }

 protected:
  IntervalTrigger *parent_;
};

template<typename... Ts> class IntervalStopAction : public Action<Ts...> {
 public:
  IntervalStopAction(IntervalTrigger *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(const char *, url)
  void play(Ts... x) override { this->parent_->stop(); }

 protected:
  IntervalTrigger *parent_;
};

}  // namespace interval
}  // namespace esphome
