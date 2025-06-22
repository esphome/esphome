#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace loop_test_component {

static const char *const TAG = "loop_test_component";

class LoopTestComponent : public Component {
 public:
  void set_name(const std::string &name) { this->name_ = name; }
  void set_disable_after(int count) { this->disable_after_ = count; }
  void set_test_redundant_operations(bool test) { this->test_redundant_operations_ = test; }

  void setup() override;
  void loop() override;

  // Service methods for external control
  void service_enable();
  void service_disable();

  int get_loop_count() const { return this->loop_count_; }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::string name_;
  int loop_count_{0};
  int disable_after_{0};
  bool test_redundant_operations_{false};
};

template<typename... Ts> class EnableAction : public Action<Ts...> {
 public:
  EnableAction(LoopTestComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->service_enable(); }

 protected:
  LoopTestComponent *parent_;
};

template<typename... Ts> class DisableAction : public Action<Ts...> {
 public:
  DisableAction(LoopTestComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->service_disable(); }

 protected:
  LoopTestComponent *parent_;
};

}  // namespace loop_test_component
}  // namespace esphome
